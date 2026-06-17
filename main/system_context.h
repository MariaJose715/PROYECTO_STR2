#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H
/**
 * ================================================================
 * CONTEXTO GLOBAL DEL SISTEMA
 * ================================================================
 *
 * Este archivo define una estructura única (system_context_t) que
 * centraliza todo el estado compartido del proyecto.
 *
 * ¿Por qué un contexto único?
 * En lugar de usar variables globales (static en cada archivo .c),
 * toda la información del sistema se guarda en una sola estructura
 * que se pasa como puntero a todas las funciones que la necesitan.
 *
 * Esto hace que el código sea más predecible, reutilizable y fácil
 * de entender: cualquier función que modifique el estado del sistema
 * recibe ctx como parámetro, dejando claro qué datos usa.
 *
 * La estructura se crea en app_main() con calloc() en el montón
 * (heap) y se distribuye a:
 *   - Las tareas FreeRTOS (a través de su parámetro void *param)
 *   - Los manejadores del servidor HTTP (a través de req->user_ctx)
 *   - Los manejadores de eventos WiFi (a través de void *arg)
 *   - Llamadas directas a funciones de cada módulo
 * ================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

// Máximo de horarios programables para la cortina
#define MAX_CURTAIN_SCHEDULES 8

/**
 * curtain_schedule_t
 * Representa un único horario de apertura de cortina.
 * El usuario puede definir hasta MAX_CURTAIN_SCHEDULES de estos
 * a través del panel web.
 */
typedef struct {
    uint8_t hora;           // Hora del evento (0-23)
    uint8_t minuto;         // Minuto del evento (0-59)
    uint8_t apertura;       // Apertura de cortina en % (0-100)
    bool    activo;         // true si el horario está habilitado
} curtain_schedule_t;

/**
 * system_context_t
 * Contiene TODAS las variables que antes eran globales estáticas
 * distribuidas en los diferentes módulos.
 *
 * Organización (por módulo de origen):
 *   1. ADC      - Manejadores del conversor analógico-digital
 *   2. PWM      - Velocidad actual del ventilador
 *   3. WiFi     - Estado de red, IP, grupo de eventos
 *   4. Web      - Temperaturas, modos, RGB, horarios, servidor HTTP
 *   5. Main     - Manejador de tarea de alarma, flag de hora NTP
 */
typedef struct system_context {

    // ---- ADC (adc_handler.c) ----
    // Manejadores del driver oneshot del ADC, necesarios para leer
    // el sensor de temperatura conectado al canal 4.
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;

    // ---- PWM (pwm_handler.c) ----
    // Última velocidad asignada al ventilador, para poder reportarla
    // en la API REST sin tener que leer el registro del LEDC.
    uint8_t fan_speed_actual;

    // ---- WiFi (wifi_handler.c) ----
    char ip_sta[16];                    // Dirección IP en modo Station
    bool wifi_conectado;                // Flag de conexión Station activa
    int s_retry_num;                    // Contador de reintentos al conectar
    EventGroupHandle_t wifi_event_group; // Grupo de eventos para sincronizar la conexión

    // ---- Web Server (web_server.c) ----
    float temperatura_actual;           // Última lectura del sensor
    float temp_deseada;                 // Temperatura deseada por el usuario
    float temp_maxima;                  // Temperatura máxima permitida
    bool fan_auto_mode;                 // true = automático, false = manual
    uint8_t fan_speed_manual;           // Velocidad manual del ventilador (0-100%)
    bool curtain_auto_mode;             // true = programado, false = manual
    uint8_t curtain_position;           // Posición manual de cortina (0-100%)
    uint8_t rgb_r, rgb_g, rgb_b;        // Componentes de color del LED RGB
    uint8_t rgb_brillo;                 // Brillo del LED RGB (0-100%)
    curtain_schedule_t schedules[MAX_CURTAIN_SCHEDULES]; // Arreglo de horarios
    int schedule_count;                 // Cuántos horarios están en uso
    httpd_handle_t server;              // Manejador del servidor HTTP

    // ---- Main (main.c) ----
    TaskHandle_t tarea_alarma_handle;   // Manejador de la tarea de parpadeo de alarma
    bool hora_sincronizada;             // true cuando el NTP respondió exitosamente

} system_context_t;

#endif
