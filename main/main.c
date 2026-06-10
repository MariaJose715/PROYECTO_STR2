/**
 * ================================================================
 * PROYECTO STR 2026 - SISTEMA DE CONTROL AMBIENTAL AUTOMATIZADO
 * ================================================================
 *
 * Descripción General:
 * Este sistema IoT controla la ventilación, iluminación ambiental y
 * cortinas de forma automatizada. Se opera de forma remota a través
 * de una interfaz web intuitiva, con soporte para configuración
 * dinámica de red y actualizaciones OTA (Over-The-Air).
 *
 * Componentes Físicos:
 *   - Ventilador (PWM)         -> Control de ventilación proporcional
 *   - Servomotor               -> Apertura/cierre de cortinas
 *   - Sensor de temperatura    -> LM35 (lectura por ADC)
 *   - LED RGB                  -> Iluminación ambiental
 *   - LED Rojo                 -> Alarma de temperatura
 *   - ESP32                    -> Microcontrolador con WiFi
 *
 * Funcionalidades Implementadas:
 *   1. Control de cortinas (programado y manual)
 *   2. Control de temperatura y ventilación (automático y manual)
 *   3. Iluminación RGB (color y brillo configurables)
 *   4. Servidor web con panel de control
 *   5. Configuración dinámica de WiFi (Station y AP)
 *   6. Actualizaciones OTA
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

// Módulos del proyecto
#include "gpio_handler.h"     // Definición de pines GPIO
#include "uart_handler.h"     // Depuración por serial
#include "pwm_handler.h"      // PWM para ventilador y servo
#include "adc_handler.h"      // ADC para sensor de temperatura
#include "led_handler.h"      // LED RGB y LED de alarma
#include "wifi_handler.h"     // WiFi modo Station y AP
#include "web_server.h"       // Servidor web HTTP
#include "ota_handler.h"      // Actualizaciones OTA

// ================================================================
// CONSTANTES DEL SISTEMA
// ================================================================

#define INTERVALO_LECTURA_TEMP_MS    2000    // Leer temperatura cada 2 segundos
#define INTERVALO_ALARMA_MS          1000    // Período de parpadeo de alarma (1 Hz)
#define INTERVALO_CURTAIN_CHECK_MS   30000   // Verificar horarios cada 30 segundos

// ================================================================
// VARIABLES GLOBALES
// ================================================================

static TaskHandle_t tarea_alarma_handle = NULL;  // Manejador de la tarea de alarma

// ================================================================
// FUNCIONES AUXILIARES
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
 */
static void controlar_ventilador_automatico(float temp_actual)
{
    float t_des = web_get_temp_deseada();
    float t_max = web_get_temp_maxima();

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
        // Control proporcional: mapear linealmente
        float proporcion = (temp_actual - t_des) / (t_max - t_des);
        speed = (uint8_t)(proporcion * 100.0f);
        if (speed > 100) speed = 100;
    }

    pwm_fan_set_speed(speed);
}

/**
 * verificar_alarma_temperatura()
 * Verifica si la temperatura superó el máximo y activa/desactiva la alarma.
 * La alarma consiste en un LED rojo parpadeando a 1 Hz.
 *
 * Retorna: true si hay alarma activa.
 */
static bool verificar_alarma_temperatura(float temp_actual)
{
    float t_max = web_get_temp_maxima();

    if (temp_actual > t_max)
    {
        // Activar alarma si no está ya activa
        if (tarea_alarma_handle == NULL)
        {
            // Crear tarea de parpadeo del LED de alarma
            xTaskCreate(
                led_alarma_parpadeo_task,
                "alarma_led",
                2048,
                NULL,
                5,
                &tarea_alarma_handle
            );
            uart_send_msg("[SISTEMA] ⚠ ALARMA: Temperatura (%.1f°C) excede máximo (%.1f°C)",
                          temp_actual, t_max);
        }
        return true;
    }
    else
    {
        // Desactivar alarma si estaba activa
        if (tarea_alarma_handle != NULL)
        {
            vTaskDelete(tarea_alarma_handle);
            tarea_alarma_handle = NULL;
            led_alarma_set(false);  // Asegurar que el LED quede apagado
            uart_send_msg("[SISTEMA] Alarma desactivada. Temperatura normal: %.1f°C", temp_actual);
        }
        return false;
    }
}

/**
 * verificar_horarios_cortina()
 * Revisa si algún horario programado de cortina debe ejecutarse.
 * Compara la hora actual con los registros guardados.
 */
static void verificar_horarios_cortina(void)
{
    if (!web_get_curtain_auto_mode()) return;  // No verificar si está en modo manual

    // Obtener la hora actual del sistema
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hora_actual = timeinfo.tm_hour;
    int min_actual  = timeinfo.tm_min;

    // Obtener los horarios guardados
    curtain_schedule_t scheds[MAX_CURTAIN_SCHEDULES];
    int count;
    web_get_schedules(scheds, &count);

    for (int i = 0; i < count; i++)
    {
        if (scheds[i].activo &&
            scheds[i].hora == hora_actual &&
            scheds[i].minuto == min_actual)
        {
            // Ejecutar la acción programada
            pwm_servo_set_position(scheds[i].apertura);
            web_set_curtain_position(scheds[i].apertura);
            uart_send_msg("[SISTEMA] Cortina programada: %d%% a las %02d:%02d",
                          scheds[i].apertura, scheds[i].hora, scheds[i].minuto);
        }
    }
}

/**
 * inicializar_hora_sistema()
 * Configura la hora del sistema.
 * En un sistema real se sincronizaría con un servidor NTP.
 * Aquí usamos una hora simulada para propósitos de demostración.
 */
static void inicializar_hora_sistema(void)
{
    // Configurar zona horaria (UTC-6 para Centroamérica, ajustar según sea necesario)
    setenv("TZ", "CST-6", 1);
    tzset();

    // Configurar hora inicial (simulada - en producción usar NTP)
    struct tm tm_init = {
        .tm_year  = 2026 - 1900,
        .tm_mon   = 5,        // Junio (0-indexed)
        .tm_mday  = 9,
        .tm_hour  = 12,
        .tm_min   = 0,
        .tm_sec   = 0,
    };
    time_t t = mktime(&tm_init);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);

    uart_send_msg("[SISTEMA] Hora del sistema inicializada");
}

// ================================================================
// TAREA PRINCIPAL DE CONTROL
// ================================================================

/**
 * tarea_control_ambiental()
 * Tarea principal que ejecuta el bucle de control del sistema.
 * Se encarga de:
 *   1. Leer la temperatura del sensor ADC
 *   2. Controlar el ventilador (modo automático o manual)
 *   3. Verificar la alarma de temperatura
 *   4. Controlar la cortina (modo programado o manual)
 *   5. Actualizar la interfaz web con los datos
 *
 * param: no utilizado (puede ser NULL).
 */
static void tarea_control_ambiental(void *param)
{
    int contador_verificaciones = 0;

    while (1)
    {
        // ============================================
        // 1. LEER TEMPERATURA
        // ============================================
        float temperatura = adc_temp_read_celsius();
        web_set_temperatura_actual(temperatura);

        uart_send_msg("[SISTEMA] Temp: %.1f°C | Deseada: %.1f°C | Max: %.1f°C",
                      temperatura, web_get_temp_deseada(), web_get_temp_maxima());

        // ============================================
        // 2. CONTROLAR VENTILADOR
        // ============================================
        if (web_get_fan_auto_mode())
        {
            // Modo automático: control proporcional según temperatura
            controlar_ventilador_automatico(temperatura);
        }
        else
        {
            // Modo manual: usar la velocidad definida por el usuario
            pwm_fan_set_speed(web_get_fan_speed_manual());
        }

        // ============================================
        // 3. VERIFICAR ALARMA DE TEMPERATURA
        // ============================================
        verificar_alarma_temperatura(temperatura);

        // ============================================
        // 4. CONTROLAR CORTINA
        // ============================================
        if (web_get_curtain_auto_mode())
        {
            // Verificar horarios cada cierto tiempo
            if (contador_verificaciones % (INTERVALO_CURTAIN_CHECK_MS / INTERVALO_LECTURA_TEMP_MS) == 0)
            {
                verificar_horarios_cortina();
            }
        }
        else
        {
            // Modo manual: usar la posición definida por el usuario
            pwm_servo_set_position(web_get_curtain_position());
        }

        contador_verificaciones++;

        // ============================================
        // 5. ESPERAR SIGUIENTE CICLO
        // ============================================
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_LECTURA_TEMP_MS));
    }
}

// ================================================================
// PUNTO DE ENTRADA PRINCIPAL
// ================================================================

/**
 * app_main()
 * Función principal del programa (equivalente a main() en ESP-IDF).
 * Inicializa todos los módulos del sistema y arranca las tareas.
 *
 * Orden de inicialización:
 *   1. GPIO      - Configurar pines
 *   2. UART      - Comunicación serial para depuración
 *   3. ADC       - Sensor de temperatura
 *   4. PWM       - Ventilador y servomotor
 *   5. LED       - RGB y alarma
 *   6. OTA       - Actualizaciones inalámbricas
 *   7. Hora      - Configurar reloj del sistema
 *   8. WiFi      - Red y punto de acceso
 *   9. Web Server - Interfaz de control
 *   10. Tarea de control - Bucle principal
 */
void app_main(void)
{
    // ============================================
    // 1. Inicializar GPIO (pines del sistema)
    // ============================================
    gpio_init_all();
    uart_init();
    uart_send_msg("========================================");
    uart_send_msg("  STR 2026 - Control Ambiental v1.0");
    uart_send_msg("  Inicializando sistema...");
    uart_send_msg("========================================");

    // ============================================
    // 2. Inicializar módulos de hardware
    // ============================================
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
    // 3. Inicializar OTA
    // ============================================
    ota_init();
    uart_send_msg("[OK] OTA inicializado");

    // ============================================
    // 4. Configurar hora del sistema
    // ============================================
    inicializar_hora_sistema();

    // ============================================
    // 5. Inicializar WiFi (AP + Station)
    // ============================================
    wifi_init();

    // Pequeña pausa para que WiFi termine de configurarse
    vTaskDelay(pdMS_TO_TICKS(2000));

    // ============================================
    // 6. Iniciar servidor web
    // ============================================
    web_server_start();

    // ============================================
    // 7. Crear tarea principal de control
    // ============================================
    xTaskCreate(
        tarea_control_ambiental,
        "control_ambiental",
        4096,       // Stack size (4 KB)
        NULL,
        5,          // Prioridad
        NULL
    );

    uart_send_msg("[OK] Sistema listo. Conéctese al AP o a la IP del dispositivo.");
    uart_send_msg("[OK] Panel web disponible en http://192.168.4.1 (AP)");
}
