/**
 * ================================================================
 * PROYECTO STR 2026 - SISTEMA DE CONTROL AMBIENTAL AUTOMATIZADO
 * ================================================================
 *
 *   Este es el archivo principal del firmware. Aquí se encuentra
 *   la función app_main() que es el punto de entrada del programa
 *   (equivalente a main() en un programa de escritorio).
 *
 *   ¿Qué hace este sistema?
 *   - Lee la temperatura ambiente con un NTC (ADC)
 *   - Controla un ventilador (PWM) de forma proporcional a la temp
 *   - Abre/cierra cortinas con un servomotor (PWM)
 *   - Ilumina con un LED RGB (3 canales PWM)
 *   - Enciende un LED rojo de alarma si hace demasiado calor
 *   - Sirve un panel web para monitorear y controlar todo
 *   - Se conecta a WiFi (modo Station) o crea su propia red (AP)
 *   - Sincroniza la hora con NTP para ejecutar horarios
 *   - Soporta actualizaciones OTA (Over-The-Air)
 *
 *   Arquitectura de tareas FreeRTOS:
 *     Tarea "control_ambiental" (prioridad 5, stack 4096):
 *       Ejecuta el bucle principal cada 2 segundos.
 *       Lee temperatura, controla ventilador, verifica alarma y cortinas.
 *
 *     Tarea "alarma_led" (prioridad 5, stack 2048):
 *       Se crea solo cuando hay alarma. Parpadea LED rojo a 1 Hz.
 *       Se elimina cuando la temperatura vuelve a la normalidad.
 *
 *     Tarea "ota_update_task" (prioridad 5, stack 8192):
 *       Se crea al iniciar una actualización OTA.
 *       Descarga el firmware .bin desde una URL y lo flashea.
 *       Se elimina sola al terminar (o reinicia el sistema).
 * ================================================================
 */

// ================================================================
// BIBLIOTECAS ESTÁNDAR
// ================================================================
#include <stdio.h>       // sprintf, snprintf
#include <stdlib.h>      // malloc, free
#include <string.h>      // strcpy, strlen
#include <time.h>        // time(), localtime_r(), strftime()
#include <sys/time.h>    // settimeofday(), struct timeval

// FreeRTOS - Sistema operativo de tiempo real del ESP32
// Permite crear múltiples tareas que se ejecutan "al mismo tiempo"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LwIP SNTP - Protocolo para sincronizar la hora con servidores de internet
// Se usa para obtener la hora real después de que WiFi se conecta
#include "lwip/apps/sntp.h"

// ================================================================
// MÓDULOS PROPIOS DEL PROYECTO
// Cada archivo .c/.h maneja un periférico o funcionalidad específica
// ================================================================
#include "gpio_handler.h"     // Definición de pines GPIO del ESP32
#include "uart_handler.h"     // Comunicación serial para depuración
#include "pwm_handler.h"      // PWM para ventilador (25kHz) y servo (50Hz)
#include "adc_handler.h"      // ADC para leer el sensor NTC de temperatura
#include "led_handler.h"      // LED RGB (PWM) y LED de alarma (ON/OFF)
#include "wifi_handler.h"     // WiFi modo Station (cliente) + AP (punto de acceso)
#include "web_server.h"       // Servidor web HTTP con API REST y dashboard
#include "ota_handler.h"      // Actualizaciones de firmware inalámbricas

// ================================================================
// CONSTANTES DEL SISTEMA
// ================================================================
// Estas constantes controlan la frecuencia de cada acción en el bucle principal.
// Se definen como #define para que el compilador las reemplace directamente
// (sin ocupar memoria RAM).

#define INTERVALO_LECTURA_TEMP_MS    2000    // Cada 2 segundos: leer temperatura y actuar
#define INTERVALO_ALARMA_MS          1000    // Período de parpadeo del LED de alarma (1 Hz)
                                            // 1000ms encendido + 1000ms apagado = 2 seg/ciclo
#define INTERVALO_CURTAIN_CHECK_MS   30000   // Cada 30 segundos: verificar horarios de cortina
                                            // No necesitamos verificarlo cada 2 segundos

// ================================================================
// VARIABLES GLOBALES
// ================================================================
// Son "static" para que solo este archivo pueda acceder a ellas
// (encapsulamiento básico en C).

static TaskHandle_t tarea_alarma_handle = NULL;
    // Manejador (identificador) de la tarea de parpadeo del LED de alarma.
    // Cuando la temperatura supera el máximo, creamos una tarea que parpadea el LED.
    // Guardamos su "TaskHandle" para poder eliminarla cuando la temperatura baje.
    // Inicia en NULL porque al arranque no hay alarma.

static bool hora_sincronizada = false;
    // Bandera que indica si el reloj del sistema tiene la hora correcta (vía NTP).
    // Se vuelve "true" cuando NTP responde exitosamente.
    // Se consulta desde web_server.c para mostrarlo en el dashboard.

// ================================================================
// FUNCIONES AUXILIARES DE CONTROL
// ================================================================
// Estas funciones implementan la lógica de control del sistema.
// Son "static" porque solo se usan dentro de este archivo.

/**
 * controlar_ventilador_automatico()
 * ---------------------------------------------------------------
 * Calcula la velocidad del ventilador según la temperatura actual
 * usando un control proporcional simple (sin PID).
 *
 * ¿Cómo funciona el control proporcional?
 *   Es como un dimmer que se ajusta solo:
 *   - Si la temperatura está en el valor deseado -> ventilador apagado (0%)
 *   - Si la temperatura llega al máximo -> ventilador al 100%
 *   - Si está entre medio -> la velocidad es proporcional:
 *       Ej: T_deseada=25°C, T_max=35°C, si T=30°C:
 *           velocidad = ((30-25)/(35-25)) * 100 = 50%
 *
 * Parámetros:
 *   temp_actual: temperatura leída del sensor NTC (en °C)
 *
 * No retorna nada, pero actualiza el PWM del ventilador.
 */
static void controlar_ventilador_automatico(float temp_actual)
{
    // Obtener las temperaturas configuradas por el usuario desde el servidor web
    float t_des = web_get_temp_deseada();  // Temperatura que el usuario quiere (ej: 25°C)
    float t_max = web_get_temp_maxima();   // Temp máxima antes de alarma (ej: 35°C)

    uint8_t speed = 0;  // Velocidad del ventilador en porcentaje (0-100)

    if (temp_actual <= t_des)
    {
        // CASO 1: Estamos en la temperatura ideal o más frío
        // El ventilador no necesita trabajar
        speed = 0;
    }
    else if (temp_actual >= t_max)
    {
        // CASO 2: Hace demasiado calor, llegamos al límite
        // Ventilador a máxima potencia
        speed = 100;
    }
    else
    {
        // CASO 3: Temperatura intermedia - control proporcional
        // Calculamos qué tan cerca estamos del máximo
        float proporcion = (temp_actual - t_des) / (t_max - t_des);
        // Ej: si t_des=25, t_max=35, temp=30
        //     proporcion = (30-25)/(35-25) = 5/10 = 0.5 (50%)

        speed = (uint8_t)(proporcion * 100.0f);  // Convertir a porcentaje (0-100)
        if (speed > 100) speed = 100;  // Protección por si hay error de redondeo
    }

    // Enviar la velocidad calculada al módulo PWM
    pwm_fan_set_speed(speed);
}

/**
 * verificar_alarma_temperatura()
 * ---------------------------------------------------------------
 * Revisa si la temperatura superó el máximo permitido.
 * Si es así, activa el LED de alarma parpadeante.
 * Cuando la temperatura vuelve a la normalidad, apaga la alarma.
 *
 * ¿Cómo funciona la activación/desactivación?
 *   - La primera vez que T > T_max, crea una tarea FreeRTOS
 *     que parpadea el LED (1 segundo encendido, 1 apagado).
 *   - Cuando T vuelve a ser <= T_max, elimina esa tarea
 *     y apaga el LED.
 *
 * Parámetros:
 *   temp_actual: temperatura actual del sensor
 *
 * Retorna:
 *   true  = hay alarma activa (T > T_max)
 *   false = temperatura normal
 */
static bool verificar_alarma_temperatura(float temp_actual)
{
    float t_max = web_get_temp_maxima();

    if (temp_actual > t_max)
    {
        // ----- ACTIVAR ALARMA -----
        // Usamos el TaskHandle para saber si ya estábamos en alarma.
        // Si es NULL, significa que es la primera vez que detectamos
        // la temperatura alta en este ciclo.
        if (tarea_alarma_handle == NULL)
        {
            // Crear una tarea de FreeRTOS que parpadea el LED
            // La tarea ejecuta led_alarma_parpadeo_task() en un bucle infinito
            // hasta que la eliminemos con vTaskDelete()
            xTaskCreate(
                led_alarma_parpadeo_task,  // Función que ejecuta la tarea
                "alarma_led",              // Nombre (para depuración)
                2048,                      // Tamaño de pila (2 KB)
                NULL,                      // Parámetro (ninguno)
                5,                         // Prioridad (5 = misma que control)
                &tarea_alarma_handle       // Aquí se guarda el identificador
            );
            uart_send_msg("[SISTEMA] ALARMA: Temperatura (%.1f°C) excede máximo (%.1f°C)",
                          temp_actual, t_max);
        }
        return true;  // Indicar que hay alarma
    }
    else
    {
        // ----- DESACTIVAR ALARMA -----
        // Solo si la tarea existe (handle != NULL) la eliminamos
        if (tarea_alarma_handle != NULL)
        {
            vTaskDelete(tarea_alarma_handle);   // Eliminar la tarea de parpadeo
            tarea_alarma_handle = NULL;          // Marcar que ya no hay alarma
            led_alarma_set(false);               // Apagar el LED por si quedó encendido
            uart_send_msg("[SISTEMA] Alarma desactivada. Temperatura normal: %.1f°C", temp_actual);
        }
        return false;  // Indicar que NO hay alarma
    }
}

/**
 * verificar_horarios_cortina()
 * ---------------------------------------------------------------
 * Revisa si la hora actual coincide con algún horario programado
 * para las cortinas. Si coincide, mueve el servo a la posición
 * guardada en ese horario.
 *
 * ¿Cómo funciona?
 *   1. Solo revisa si el modo cortina está en "Automático/Programado"
 *   2. Obtiene la hora actual del sistema (hora y minuto)
 *   3. Compara contra todos los horarios guardados (máx. 8)
 *   4. Si encuentra coincidencia exacta (hora Y minuto),
 *      mueve el servo y guarda la posición
 *
 * Esta función se llama cada ~30 segundos desde el bucle principal.
 */
static void verificar_horarios_cortina(void)
{
    // Si el usuario puso las cortinas en modo manual, no hacemos nada
    if (!web_get_curtain_auto_mode()) return;

    // --- Obtener la hora actual del sistema ---
    // time() devuelve los segundos desde 1970 (Unix timestamp)
    // localtime_r() convierte ese número a hora legible (hora, minuto, segundo)
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hora_actual = timeinfo.tm_hour;  // 0-23
    int min_actual  = timeinfo.tm_min;   // 0-59

    // --- Obtener los horarios guardados por el usuario desde el servidor web ---
    curtain_schedule_t scheds[MAX_CURTAIN_SCHEDULES];  // Arreglo para copiar los horarios
    int count;                                           // Cuántos horarios hay guardados
    web_get_schedules(scheds, &count);

    // Recorrer todos los horarios buscando coincidencias
    for (int i = 0; i < count; i++)
    {
        // Solo ejecutar si el horario está activo Y coincide hora/minuto
        if (scheds[i].activo &&
            scheds[i].hora == hora_actual &&
            scheds[i].minuto == min_actual)
        {
            // Ejecutar: mover el servo a la posición programada
            pwm_servo_set_position(scheds[i].apertura);
            // Actualizar la posición en el servidor web para que el dashboard lo muestre
            web_set_curtain_position(scheds[i].apertura);
            uart_send_msg("[SISTEMA] Cortina programada: %d%% a las %02d:%02d",
                          scheds[i].apertura, scheds[i].hora, scheds[i].minuto);
        }
    }

    // --- Depuración: mostrar la hora cada ~30 segundos ---
    // La variable static "dbg" mantiene su valor entre llamadas a esta función
    static int dbg = 0;
    if (++dbg % 15 == 0)  // 15 ciclos * 2 segundos = 30 segundos
    {
        char buf[64];
        struct tm ti;
        localtime_r(&now, &ti);
        strftime(buf, sizeof(buf), "%H:%M:%S", &ti);  // Formato: "14:30:00"
        uart_send_msg("[DBG] Hora sistema: %s | Cortinas: %d programadas", buf, count);
    }
}

/**
 * inicializar_hora_fallback()
 * ---------------------------------------------------------------
 * Configura una hora por defecto cuando NTP no está disponible.
 * Esto permite que el sistema funcione aunque no haya internet.
 *
 * ¿Por qué necesitamos esto?
 *   - El sistema usa la hora para ejecutar los horarios de cortina
 *   - Si no hay WiFi o los servidores NTP no responden,
 *     ponemos una hora fija para que el sistema pueda funcionar
 *   - El usuario puede cambiarla manualmente con POST /api/time/set
 */
static void inicializar_hora_fallback(void)
{
    // Crear una estructura con la fecha/hora por defecto
    // struct tm: formato que usa C para representar fecha/hora
    struct tm tm_init = {
        .tm_year  = 2026 - 1900,  // año = 2026 (se resta 1900 porque así lo requiere C)
        .tm_mon   = 5,             // mes = Junio (0=enero, 5=junio)
        .tm_mday  = 10,            // día = 10
        .tm_hour  = 8,             // hora = 08:00:00
        .tm_min   = 0,
        .tm_sec   = 0,
    };

    // mktime() convierte struct tm a time_t (segundos desde 1970)
    time_t t = mktime(&tm_init);

    // settimeofday() configura el reloj interno del ESP32
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);

    // Mostrar la hora configurada por UART para depuración
    char buf[64];
    struct tm ti;
    localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
    uart_send_msg("[SISTEMA] Hora FALLBACK (NTP no disponible): %s", buf);
}

/**
 * sincronizar_hora_ntp()
 * ---------------------------------------------------------------
 * Intenta sincronizar el reloj del ESP32 con servidores NTP
 * (Network Time Protocol) de internet.
 *
 * ¿Cómo funciona NTP?
 *   1. Configuramos el cliente SNTP (Simple NTP) de LwIP
 *   2. Le decimos que consulte "pool.ntp.org" y "time.google.com"
 *   3. Iniciamos el cliente y esperamos hasta 15 segundos
 *   4. Verificamos con time() si la hora ya es válida (> 2025)
 *   5. Si funciona, hora_sincronizada = true y mostramos la hora
 *   6. Si no, detenemos SNTP y retornamos false
 *
 * Retorna:
 *   true  = hora sincronizada correctamente
 *   false = timeout, no se pudo sincronizar
 */
static bool sincronizar_hora_ntp(void)
{
    // --- Configurar zona horaria ---
    // "CST-6" = Central Standard Time, UTC-6 (Centroamérica)
    // El 6 significa que estamos 6 horas detrás de UTC
    setenv("TZ", "CST-6", 1);
    tzset();

    uart_send_msg("[NTP] Iniciando sincronización...");

    // --- Configurar el cliente SNTP ---
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
        // SNTP_OPMODE_POLL: el ESP32 pregunta periódicamente la hora

    sntp_setservername(0, "pool.ntp.org");
        // pool.ntp.org es un servicio que redirige a servidores NTP cercanos

    sntp_setservername(1, "time.google.com");
        // Respaldo: servidor NTP de Google

    sntp_init();
        // Iniciar el cliente SNTP (comienza a hacer consultas)

    // --- Esperar hasta que la hora sea válida ---
    // Una hora válida es cualquier fecha después del 1 de enero de 2025
    // Esto evita confundir con el valor por defecto (1970-01-01)
    const time_t MIN_VALID = 1735689600;  // 1-Jan-2025 00:00:00 UTC en segundos Unix
    const int TIMEOUT_MS = 15000;          // Esperar máximo 15 segundos
    int espera = 0;

    while (espera < TIMEOUT_MS)
    {
        time_t now = 0;
        time(&now);  // Preguntar la hora actual al sistema

        if (now > MIN_VALID)
        {
            // ¡Hora sincronizada!
            hora_sincronizada = true;

            char buf[64];
            struct tm ti;
            localtime_r(&now, &ti);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
            uart_send_msg("[NTP] Hora sincronizada: %s", buf);
            return true;
        }

        // No hay hora válida aún, esperar 500ms y volver a preguntar
        vTaskDelay(pdMS_TO_TICKS(500));
        espera += 500;
    }

    // Si llegamos aquí, es porque pasaron 15 segundos sin respuesta
    uart_send_msg("[NTP] Timeout - no se pudo sincronizar");
    sntp_stop();  // Detener el cliente SNTP para ahorrar recursos
    return false;
}

/**
 * web_hora_sincronizada()
 * ---------------------------------------------------------------
 * Función pública (declarada en web_server.h) que permite a otros
 * módulos (como web_server.c) saber si la hora fue sincronizada
 * correctamente vía NTP.
 *
 * Retorna:
 *   true  = hora sincronizada con NTP
 *   false = usando hora de fallback
 */
bool web_hora_sincronizada(void)
{
    return hora_sincronizada;
}

// ================================================================
// TAREA PRINCIPAL DE CONTROL (FreeRTOS)
// ================================================================
//
// Esta es una de las 3 tareas FreeRTOS del sistema:
//
//   Tarea "control_ambiental"  (esta)
//   ├── Cada 2 segundos:
//   │   1. Lee temperatura del NTC
//   │   2. Controla ventilador (auto/proporcional o manual)
//   │   3. Verifica alarma de temperatura
//   │   4. Controla cortina (programado o manual)
//   │   5. Actualiza datos en el servidor web
//   │   6. Espera 2 segundos y repite
//   │
//   └── Cada 30 segundos también verifica horarios de cortina
//

/**
 * tarea_control_ambiental()
 * ---------------------------------------------------------------
 * Función principal de la tarea de control FreeRTOS.
 * Se ejecuta en un bucle infinito (while(1)).
 *
 * ¿Por qué es una tarea separada?
 *   - app_main() solo inicializa y luego termina
 *   - El bucle de control necesita ejecutarse permanentemente
 *   - FreeRTOS se encarga de darle tiempo de CPU periódicamente
 *
 * Parámetro:
 *   param: no se usa (puede ser NULL)
 */
static void tarea_control_ambiental(void *param)
{
    // Contador de iteraciones del bucle
    // Se usa para saber cuándo verificar horarios (cada 15 iteraciones = 30s)
    int contador_verificaciones = 0;

    // --- BUCLE INFINITO DE CONTROL ---
    // Este bucle se ejecuta para siempre mientras el ESP32 esté encendido
    while (1)
    {
        // ============================================
        // PASO 1: LEER TEMPERATURA
        // ============================================
        // El ADC mide el voltaje en el divisor NTC+resistor
        // y la función adc_temp_read_celsius() lo convierte a °C
        float temperatura = adc_temp_read_celsius();

        // Guardar la temperatura en el servidor web para que
        // el dashboard pueda mostrarla cuando el navegador la pida
        web_set_temperatura_actual(temperatura);

        // Mostrar en la consola serial (para depuración)
        uart_send_msg("[SISTEMA] Temp: %.1f°C | Deseada: %.1f°C | Max: %.1f°C",
                      temperatura, web_get_temp_deseada(), web_get_temp_maxima());

        // ============================================
        // PASO 2: CONTROLAR VENTILADOR
        // ============================================
        // El usuario puede elegir entre modo automático o manual
        // desde el panel web (dashboard)
        if (web_get_fan_auto_mode())
        {
            // MODO AUTOMÁTICO: el sistema decide la velocidad
            // según la temperatura (control proporcional)
            controlar_ventilador_automatico(temperatura);
        }
        else
        {
            // MODO MANUAL: el usuario fijó una velocidad específica
            // desde el dashboard
            pwm_fan_set_speed(web_get_fan_speed_manual());
        }

        // ============================================
        // PASO 3: VERIFICAR ALARMA DE TEMPERATURA
        // ============================================
        // Si la temperatura supera el máximo:
        //   - Crea una tarea que parpadea el LED rojo
        // Si la temperatura vuelve a la normalidad:
        //   - Elimina la tarea y apaga el LED
        verificar_alarma_temperatura(temperatura);

        // ============================================
        // PASO 4: CONTROLAR CORTINA
        // ============================================
        if (web_get_curtain_auto_mode())
        {
            // MODO AUTOMÁTICO (PROGRAMADO):
            // Verificar horarios cada 30 segundos (15 iteraciones)
            // La cuenta: 30000ms / 2000ms = 15 iteraciones
            if (contador_verificaciones % (INTERVALO_CURTAIN_CHECK_MS / INTERVALO_LECTURA_TEMP_MS) == 0)
            {
                verificar_horarios_cortina();
            }
        }
        else
        {
            // MODO MANUAL: usar la posición que el usuario definió
            pwm_servo_set_position(web_get_curtain_position());
        }

        contador_verificaciones++;

        // ============================================
        // PASO 5: ESPERAR AL SIGUIENTE CICLO
        // ============================================
        // FreeRTOS suspende esta tarea durante 2 segundos.
        // Mientras tanto, otras tareas pueden ejecutarse
        // (como el servidor web o el parpadeo de alarma).
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_LECTURA_TEMP_MS));
    }
}

// ================================================================
// PUNTO DE ENTRADA PRINCIPAL (app_main)
// ================================================================
//
// En un programa normal de C, la función que se ejecuta al inicio
// se llama main(). En ESP-IDF, se llama app_main().
//
// app_main() se encarga de:
//   1. Inicializar todos los módulos (GPIO, ADC, PWM, WiFi, etc.)
//   2. Sincronizar la hora (NTP o fallback)
//   3. Arrancar el servidor web
//   4. Crear la tarea de control FreeRTOS
//
// app_main() NO debe tener un while(1) infinito porque el sistema
// operativo FreeRTOS maneja la ejecución de las tareas.
//

/**
 * app_main()
 * ---------------------------------------------------------------
 * Función principal del programa.
 * Se llama automáticamente después de que el ESP32 arranca y
 * el sistema ESP-IDF termina su inicialización interna.
 *
 * Orden de inicialización:
 *   1. GPIO      -> Configurar pines como entrada/salida
 *   2. UART      -> Comunicación serial (115200 baud)
 *   3. ADC       -> Sensor de temperatura (NTC)
 *   4. PWM       -> Ventilador (25kHz) + Servo (50Hz)
 *   5. LED       -> RGB (PWM) + Alarma (GPIO)
 *   6. OTA       -> Actualizaciones inalámbricas
 *   7. WiFi      -> AP (punto de acceso) + Station (cliente)
 *   8. Hora      -> NTP si hay WiFi, si no hora por defecto
 *   9. Web Server -> Servidor HTTP en puerto 80
 *   10. Tarea    -> Crear bucle de control FreeRTOS
 */
void app_main(void)
{
    // ============================================
    // 1. INICIALIZAR GPIO
    // ============================================
    // Configura todos los pines del ESP32:
    //   - LED RGB: GPIO5, GPIO6, GPIO7 como salida PWM
    //   - LED Alarma: GPIO8 como salida digital
    //   - Ventilador: GPIO10 como salida PWM
    //   - Servo: GPIO11 como salida PWM
    //   - ADC: GPIO4 como entrada analógica
    //   - UART: GPIO16 (TX), GPIO17 (RX)
    gpio_init_all();

    // Inicializar UART para mensajes de depuración
    uart_init();

    uart_send_msg("========================================");
    uart_send_msg("  STR 2026 - Control Ambiental v1.0");
    uart_send_msg("  Inicializando sistema...");
    uart_send_msg("========================================");

    // ============================================
    // 2. INICIALIZAR MÓDULOS DE HARDWARE
    // ============================================
    // Cada módulo configura su periférico correspondiente
    // siguiendo el patrón de los ejemplos oficiales de ESP-IDF

    adc_temp_init();
    uart_send_msg("[OK] ADC inicializado (sensor de temperatura)");

    pwm_fan_init();
    uart_send_msg("[OK] PWM ventilador inicializado");

    pwm_servo_init();
    uart_send_msg("[OK] PWM servo inicializado");

    led_rgb_init();
    led_alarma_init();
    uart_send_msg("[OK] LEDs inicializados");

    // ============================================
    // 3. INICIALIZAR OTA
    // ============================================
    // OTA permite actualizar el firmware de forma inalámbrica
    // Registra un manejador para recibir nuevas imágenes
    ota_init();
    uart_send_msg("[OK] OTA inicializado");

    // ============================================
    // 4. INICIALIZAR WiFi
    // ============================================
    // Arranca en modo AP + Station simultáneo:
    //   - AP: crea una red "maria_esp" con contraseña 12345678
    //   - Station: intenta conectarse a una red guardada en NVS
    wifi_init();

    // ============================================
    // 5. SINCRONIZAR HORA
    // ============================================
    // Primero esperamos hasta 10 segundos a que WiFi se conecte
    int espera_wifi = 0;
    while (!wifi_esta_conectado() && espera_wifi < 10000)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        espera_wifi += 500;
    }

    if (wifi_esta_conectado())
    {
        // Hay WiFi: intentar sincronizar con servidores NTP
        uart_send_msg("[SISTEMA] WiFi conectado, sincronizando hora vía NTP...");
        if (!sincronizar_hora_ntp())
        {
            // NTP falló, usar hora por defecto
            inicializar_hora_fallback();
        }
    }
    else
    {
        // No hay WiFi: usar hora por defecto
        uart_send_msg("[SISTEMA] WiFi no disponible, usando hora por defecto");
        inicializar_hora_fallback();
    }

    // ============================================
    // 6. INICIAR SERVIDOR WEB
    // ============================================
    // El servidor escucha en el puerto 80 y responde a:
    //   - Páginas web:  GET  /, /style.css, /app.js
    //   - API REST:     GET  /api/temp, /api/ping, /api/time, etc.
    //                    POST /api/fan/mode, /api/rgb, /api/time/set, etc.
    web_server_start();

    // ============================================
    // 7. CREAR TAREA PRINCIPAL DE CONTROL
    // ============================================
    // Creamos una tarea FreeRTOS que ejecutará el bucle de control
    // (leer temperatura, controlar ventilador, etc.)
    //
    // xTaskCreate recibe:
    //   1. Función a ejecutar (tarea_control_ambiental)
    //   2. Nombre de la tarea ("control_ambiental")
    //   3. Tamaño de pila en bytes (4096 = 4KB)
    //   4. Parámetro (NULL = ninguno)
    //   5. Prioridad (5 = misma que otras tareas críticas)
    //   6. Handle (NULL = no necesitamos identificador)
    xTaskCreate(
        tarea_control_ambiental,
        "control_ambiental",
        4096,
        NULL,
        5,
        NULL
    );

    uart_send_msg("[OK] Sistema listo. Conéctese al AP o a la IP del dispositivo.");
    uart_send_msg("[OK] Panel web disponible en http://192.168.4.1 (AP)");

    // app_main() termina aquí. FreeRTOS continúa ejecutando las tareas.
}
