/**
 * ================================================================
 * PROYECTO STR 2026 - SISTEMA DE CONTROL AMBIENTAL AUTOMATIZADO
 * ================================================================
 *
 * main.c — Punto de entrada y bucle principal de control
 *
 * Flujo de inicio:
 *   1. Inicializar hardware: GPIO, UART, ADC, PWM, LEDs
 *   2. Inicializar OTA (actualizaciones inalámbricas)
 *   3. Inicializar WiFi (AP + Station) con credenciales guardadas
 *   4. Sincronizar hora del sistema (NTP si hay WiFi, fallback si no)
 *   5. Arrancar servidor web HTTP con todas las rutas API
 *   6. Crear tarea FreeRTOS de control ambiental (bucle infinito)
 * ================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LwIP SNTP para sincronización de hora por red
#include "lwip/apps/sntp.h"

// Módulos del proyecto
#include "gpio_handler.h"     // Definición de pines GPIO
#include "uart_handler.h"     // Depuración por serial
#include "pwm_handler.h"      // PWM para ventilador y servo
#include "adc_handler.h"      // ADC para sensor de temperatura
#include "led_handler.h"      // LED RGB y LED de alarma
#include "wifi_handler.h"     // WiFi modo Station y AP
#include "web_server.h"       // Servidor web HTTP
#include "ota_handler.h"      // Actualizaciones OTA
#include "system_context.h"   // Contexto único del sistema

// ================================================================
// CONSTANTES DEL SISTEMA
// ================================================================

#define INTERVALO_LECTURA_TEMP_MS    2000    // Leer temperatura cada 2 segundos
#define INTERVALO_ALARMA_MS          1000    // Período de parpadeo de alarma (1 Hz)
#define INTERVALO_CURTAIN_CHECK_MS   30000   // Verificar horarios cada 30 segundos

// ================================================================
// CONTROL DE VENTILADOR (MODO AUTOMÁTICO)
// ================================================================

/**
 * controlar_ventilador_automatico()
 * Calcula la velocidad del ventilador según la temperatura actual
 * usando control proporcional.
 *
 * Lógica:
 *   - Si T <= T_deseada  -> Ventilador al 0%
 *   - Si T >= T_maxima   -> Ventilador al 100%
 *   - Si T_deseada < T < T_maxima -> Proporcional:
 *       speed = ((T - T_deseada) / (T_maxima - T_deseada)) * 100
 *
 * ctx:         contexto del sistema (para leer temperaturas de referencia)
 * temp_actual: temperatura leída del sensor en °C
 */
static void controlar_ventilador_automatico(system_context_t *ctx, float temp_actual)
{
    float t_des = web_get_temp_deseada(ctx);
    float t_max = web_get_temp_maxima(ctx);

    uint8_t speed = 0;

    if (temp_actual <= t_des)
    {
        speed = 0;  // Temperatura ideal, ventilador apagado
    }
    else if (temp_actual >= t_max)
    {
        speed = 100; // Temperatura crítica, ventilador al máximo
    }
    else
    {
        // Mapeo lineal entre T_deseada y T_maxima
        float proporcion = (temp_actual - t_des) / (t_max - t_des);
        speed = (uint8_t)(proporcion * 100.0f);
        if (speed > 100) speed = 100;
    }

    pwm_fan_set_speed(ctx, speed);
}

// ================================================================
// ALARMA DE TEMPERATURA
// ================================================================

/**
 * verificar_alarma_temperatura()
 * Compara la temperatura actual contra el máximo configurado.
 * Si lo supera, crea una tarea FreeRTOS que parpadea el LED rojo
 * a 1 Hz. Cuando la temperatura vuelve a la normalidad, elimina
 * la tarea y apaga el LED.
 *
 * ctx:         contexto del sistema (contiene el manejador de tarea)
 * temp_actual: temperatura actual en °C
 *
 * Retorna: true si la alarma está activa
 */
static bool verificar_alarma_temperatura(system_context_t *ctx, float temp_actual)
{
    float t_max = web_get_temp_maxima(ctx);

    if (temp_actual > t_max)
    {
        // Activar alarma si no está ya activa
        if (ctx->tarea_alarma_handle == NULL)
        {
            xTaskCreate(
                led_alarma_parpadeo_task,
                "alarma_led",
                2048,
                NULL,
                5,
                &ctx->tarea_alarma_handle
            );
            uart_send_msg("[SISTEMA] ⚠ ALARMA: Temperatura (%.1f°C) excede máximo (%.1f°C)",
                          temp_actual, t_max);
        }
        return true;
    }
    else
    {
        // Desactivar alarma si estaba activa
        if (ctx->tarea_alarma_handle != NULL)
        {
            vTaskDelete(ctx->tarea_alarma_handle);
            ctx->tarea_alarma_handle = NULL;
            led_alarma_set(false);  // Asegurar que el LED quede apagado
            uart_send_msg("[SISTEMA] Alarma desactivada. Temperatura normal: %.1f°C", temp_actual);
        }
        return false;
    }
}

// ================================================================
// VERIFICACIÓN DE HORARIOS DE CORTINA
// ================================================================

/**
 * verificar_horarios_cortina()
 * Revisa si algún horario programado de cortina debe ejecutarse
 * comparando la hora actual del sistema con los registros guardados
 * en el contexto. Solo actúa si el modo automático está activo.
 */
static void verificar_horarios_cortina(system_context_t *ctx)
{
    if (!web_get_curtain_auto_mode(ctx)) return;

    // Obtener la hora actual del sistema
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hora_actual = timeinfo.tm_hour;
    int min_actual  = timeinfo.tm_min;

    // Obtener los horarios guardados desde el contexto
    curtain_schedule_t scheds[MAX_CURTAIN_SCHEDULES];
    int count;
    web_get_schedules(ctx, scheds, &count);

    for (int i = 0; i < count; i++)
    {
        if (scheds[i].activo &&
            scheds[i].hora == hora_actual &&
            scheds[i].minuto == min_actual)
        {
            pwm_servo_set_position(scheds[i].apertura);
            web_set_curtain_position(ctx, scheds[i].apertura);
            uart_send_msg("[SISTEMA] Cortina programada: %d%% a las %02d:%02d",
                          scheds[i].apertura, scheds[i].hora, scheds[i].minuto);
        }
    }

    // Depuración: mostrar hora cada ~30 segundos
    static int dbg = 0;
    if (++dbg % 15 == 0)
    {
        char buf[64];
        struct tm ti;
        localtime_r(&now, &ti);
        strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
        uart_send_msg("[DBG] Hora sistema: %s | Cortinas: %d programadas", buf, count);
    }
}

// ================================================================
// SINCRONIZACIÓN DE HORA DEL SISTEMA
// ================================================================

/**
 * inicializar_hora_fallback()
 * Configura una hora por defecto (2026-06-10 08:00:00 CST-6) cuando
 * el NTP no está disponible. Esto permite que el sistema funcione
 * sin conexión a Internet, aunque los horarios de cortina dependerán
 * de la precisión de esta hora inicial.
 */
static void inicializar_hora_fallback(void)
{
    struct tm tm_init = {
        .tm_year  = 2026 - 1900,
        .tm_mon   = 5,       // Junio (0-indexed)
        .tm_mday  = 10,
        .tm_hour  = 8,
        .tm_min   = 0,
        .tm_sec   = 0,
    };
    time_t t = mktime(&tm_init);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);

    char buf[64];
    struct tm ti;
    localtime_r(&t, &ti);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
    uart_send_msg("[SISTEMA] Hora FALLBACK (NTP no disponible): %s", buf);
}

/**
 * sincronizar_hora_ntp()
 * Intenta sincronizar el reloj del sistema con servidores NTP
 * (pool.ntp.org y time.google.com como respaldo).
 *
 * Espera hasta TIMEOUT_MS por una respuesta válida. Si el NTP
 * responde, marca ctx->hora_sincronizada = true para que el
 * dashboard muestre "(NTP)" junto a la hora.
 *
 * ctx: contexto del sistema (para guardar el flag de sincronización)
 * Retorna: true si se sincronizó correctamente
 */
static bool sincronizar_hora_ntp(system_context_t *ctx)
{
    // Configurar zona horaria (UTC-6 para Centroamérica)
    setenv("TZ", "CST-6", 1);
    tzset();

    uart_send_msg("[NTP] Iniciando sincronización...");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.google.com");
    sntp_init();

    // Esperar hasta que la hora sea válida (> 1-Jan-2025 00:00:00 UTC)
    const time_t MIN_VALID = 1735689600;
    const int TIMEOUT_MS = 15000;
    int espera = 0;

    while (espera < TIMEOUT_MS)
    {
        time_t now = 0;
        time(&now);

        if (now > MIN_VALID)
        {
            ctx->hora_sincronizada = true;
            char buf[64];
            struct tm ti;
            localtime_r(&now, &ti);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
            uart_send_msg("[NTP] Hora sincronizada: %s", buf);
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
        espera += 500;
    }

    uart_send_msg("[NTP] Timeout - no se pudo sincronizar");
    sntp_stop();
    return false;
}

/**
 * web_hora_sincronizada()
 * Consulta si la hora del sistema fue obtenida vía NTP.
 * Se usa en la API REST /api/temp para mostrar "(NTP)" o "(local)"
 * en el dashboard.
 *
 * ctx: contexto del sistema
 * Retorna: true si la hora se sincronizó con NTP
 */
bool web_hora_sincronizada(system_context_t *ctx)
{
    return ctx->hora_sincronizada;
}

// ================================================================
// TAREA PRINCIPAL DE CONTROL AMBIENTAL
// ================================================================

/**
 * tarea_control_ambiental()
 * Tarea FreeRTOS que ejecuta el bucle infinito de control del sistema.
 * Se ejecuta cada INTERVALO_LECTURA_TEMP_MS (2 segundos).
 *
 * Responsabilidades por ciclo:
 *   1. Leer temperatura del sensor ADC
 *   2. Controlar velocidad del ventilador (auto o manual)
 *   3. Verificar alarma de temperatura
 *   4. Controlar cortina (horarios programados o manual)
 *
 * param: puntero al system_context_t (se pasa al crear la tarea)
 */
static void tarea_control_ambiental(void *param)
{
    system_context_t *ctx = (system_context_t *)param;
    int contador_verificaciones = 0;

    while (1)
    {
        // ---- 1. LEER TEMPERATURA ----
        float temperatura = adc_temp_read_celsius(ctx);
        web_set_temperatura_actual(ctx, temperatura);

        uart_send_msg("[SISTEMA] Temp: %.1f°C | Deseada: %.1f°C | Max: %.1f°C",
                      temperatura, web_get_temp_deseada(ctx), web_get_temp_maxima(ctx));

        // ---- 2. CONTROLAR VENTILADOR ----
        if (web_get_fan_auto_mode(ctx))
        {
            controlar_ventilador_automatico(ctx, temperatura);
        }
        else
        {
            pwm_fan_set_speed(ctx, web_get_fan_speed_manual(ctx));
        }

        // ---- 3. VERIFICAR ALARMA ----
        verificar_alarma_temperatura(ctx, temperatura);

        // ---- 4. CONTROLAR CORTINA ----
        if (web_get_curtain_auto_mode(ctx))
        {
            // Verificar horarios cada cierto número de ciclos
            if (contador_verificaciones % (INTERVALO_CURTAIN_CHECK_MS / INTERVALO_LECTURA_TEMP_MS) == 0)
            {
                verificar_horarios_cortina(ctx);
            }
        }
        else
        {
            pwm_servo_set_position(web_get_curtain_position(ctx));
        }

        contador_verificaciones++;

        // ---- 5. ESPERAR SIGUIENTE CICLO ----
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_LECTURA_TEMP_MS));
    }
}

// ================================================================
// PUNTO DE ENTRADA PRINCIPAL
// ================================================================

/**
 * app_main()
 * Función principal del programa. En ESP-IDF este es el punto de
 * entrada equivalente a main() en un programa de escritorio.
 *
 * Orden de inicialización:
 *   1. Crear el contexto del sistema en el montón (heap)
 *   2. GPIO      - Configurar pines del microcontrolador
 *   3. UART      - Comunicación serial para depuración
 *   4. ADC       - Sensor de temperatura (NTC)
 *   5. PWM       - Ventilador y servomotor de cortina
 *   6. LED       - RGB ambiental y LED rojo de alarma
 *   7. OTA       - Actualizaciones inalámbricas
 *   8. WiFi      - Modos Station y AP
 *   9. Hora      - Sincronización NTP (o fallback)
 *   10. Web Server - Interfaz de control HTTP
 *   11. Tarea de control - Bucle principal FreeRTOS
 */
void app_main(void)
{
    // ---- 0. Crear contexto del sistema en el montón ----
    // Se usa calloc() para que todos los campos inicien en cero
    // (NULL para punteros, false para booleanos, 0 para números).
    // El puntero ctx se distribuye a todos los módulos que
    // necesitan acceder al estado compartido del sistema.
    system_context_t *ctx = (system_context_t *)calloc(1, sizeof(system_context_t));
    if (ctx == NULL)
    {
        uart_send_msg("[SISTEMA] Error: no se pudo asignar memoria para el contexto");
        return;
    }

    // ---- 1 y 2. GPIO y UART ----
    gpio_init_all();
    uart_init();
    uart_send_msg("========================================");
    uart_send_msg("  STR 2026 - Control Ambiental v1.0");
    uart_send_msg("  Inicializando sistema...");
    uart_send_msg("========================================");

    // ---- 3. ADC (sensor de temperatura) ----
    adc_temp_init(ctx);
    uart_send_msg("[OK] ADC inicializado (sensor de temperatura)");

    // ---- 4. PWM (ventilador y servo) ----
    pwm_fan_init();
    uart_send_msg("[OK] PWM ventilador inicializado");

    pwm_servo_init();
    uart_send_msg("[OK] PWM servo inicializado");

    // ---- 5. LEDs (RGB y alarma) ----
    led_rgb_init();
    led_alarma_init();
    uart_send_msg("[OK] LEDs inicializados");

    // ---- 6. OTA (actualizaciones inalámbricas) ----
    ota_init();
    uart_send_msg("[OK] OTA inicializado");

    // ---- 7. WiFi ----
    wifi_init(ctx);

    // ---- 8. Hora del sistema ----
    // Esperar hasta 10 segundos a que WiFi Station se conecte
    int espera_wifi = 0;
    while (!wifi_esta_conectado(ctx) && espera_wifi < 10000)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        espera_wifi += 500;
    }

    // Intentar NTP si hay WiFi, sino usar fallback
    if (wifi_esta_conectado(ctx))
    {
        uart_send_msg("[SISTEMA] WiFi conectado, sincronizando hora vía NTP...");
        if (!sincronizar_hora_ntp(ctx))
        {
            inicializar_hora_fallback();
        }
    }
    else
    {
        uart_send_msg("[SISTEMA] WiFi no disponible, usando hora por defecto");
        inicializar_hora_fallback();
    }

    // ---- 9. Servidor web ----
    web_server_start(ctx);

    // ---- 10. Crear tarea de control ambiental ----
    // El contexto se pasa como parámetro de la tarea, así la
    // tarea_control_ambiental puede acceder a todo el estado
    // compartido sin usar variables globales.
    xTaskCreate(
        tarea_control_ambiental,
        "control_ambiental",
        4096,       // Tamaño de pila (4 KB)
        ctx,        // Parámetro: puntero al contexto
        5,          // Prioridad
        NULL        // Manejador (no necesario)
    );

    uart_send_msg("[OK] Sistema listo. Conéctese al AP o a la IP del dispositivo.");
    uart_send_msg("[OK] Panel web disponible en http://192.168.4.1 (AP)");
}
