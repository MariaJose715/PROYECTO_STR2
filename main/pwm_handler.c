/**
 * ================================================================
 * pwm_handler.c - CONTROL PWM: VENTILADOR y SERVO
 * ================================================================
 *
 * Este archivo maneja las señales PWM para dos periféricos:
 *
 *   1. VENTILADOR (25 kHz):
 *      - Alta frecuencia para evitar ruido audible en el motor
 *      - Timer 0 del LEDC, resolución 8 bits (0-255)
 *      - Velocidad ajustable de 0% a 100%
 *
 *   2. SERVO (50 Hz / 20 ms):
 *      - Baja frecuencia estándar para servomotores
 *      - Timer 1 del LEDC, resolución 13 bits (0-8191)
 *      - Posición mapeada: 0% -> 0° -> duty ~819
 *                          100% -> 180° -> duty ~1638
 *      - El servo controla la apertura de cortinas
 *
 * Distribución de timers LEDC en el ESP32-C6:
 *   Timer 0 -> Ventilador (25 kHz) [ESTE ARCHIVO]
 *   Timer 1 -> Servo       (50 Hz) [ESTE ARCHIVO]
 *   Timer 2 -> LED RGB     (1 kHz) [led_handler.c]
 *
 * Pines (definidos en gpio_handler.h):
 *   PIN_FAN_PWM   = GPIO10  -> MOSFET driver del ventilador
 *   PIN_SERVO_PWM = GPIO11  -> Señal PWM del servomotor
 *
 * Referencia: ejemplo oficial ledc_basic de ESP-IDF
 * ================================================================
 */

#include "pwm_handler.h"
#include "gpio_handler.h"
#include "driver/ledc.h"
#include "esp_err.h"

// ================================================================
// PWM del VENTILADOR (alta frecuencia ~25 kHz)
// ================================================================
//
// El ventilador usa PWM a 25 kHz porque:
//   - Los motores DC con PWM necesitan frecuencia >20 kHz para
//     evitar el "zumbido" audible (el oído humano escucha hasta ~20 kHz)
//   - A menor frecuencia, el motor vibra y hace ruido
//   - 25 kHz garantiza funcionamiento silencioso
//
// La resolución de 8 bits nos da 256 niveles de velocidad (0-255).
// Esto es suficiente para controlar un ventilador DC.

static uint8_t fan_speed_actual = 0;  // Velocidad actual del ventilador (0-100%)

// ================================================================
// pwm_fan_init()
// ================================================================
// Inicializa el timer y canal PWM para el ventilador.
//
// Timer 0 -> LEDC_LOW_SPEED_MODE (el ESP32-C6 solo soporta este modo)
// Frecuencia: 25 kHz (evita ruido audible)
// Resolución: 8 bits (duty 0-255 mapeado a 0-100%)
void pwm_fan_init(void)
{
    // Configurar temporizador LEDC
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = FAN_PWM_TIMER,        // Timer 0
        .freq_hz         = FAN_PWM_FREQ_HZ,      // 25000 Hz
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = FAN_PWM_RESOLUTION,   // 8 bits
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // Configurar canal LEDC (asocia timer + GPIO)
    ledc_channel_config_t channel_config = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = FAN_PWM_CHANNEL,       // Canal 0
        .timer_sel      = FAN_PWM_TIMER,
        .gpio_num       = PIN_FAN_PWM,           // GPIO10
        .duty           = 0,                     // Apagado al inicio
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

// ================================================================
// pwm_fan_set_speed()
// ================================================================
// Establece la velocidad del ventilador.
//
// Parámetros:
//   porcentaje: velocidad de 0% a 100%
//
// Mapeo:
//   0%  -> duty 0   (detenido)
//   50% -> duty 127 (mitad de velocidad)
//   100%-> duty 255 (máxima velocidad)
void pwm_fan_set_speed(uint8_t porcentaje)
{
    if (porcentaje > 100) porcentaje = 100;
    uint32_t duty = (porcentaje * 255) / 100;  // Mapear 0-100% a 0-255
    fan_speed_actual = porcentaje;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_PWM_CHANNEL));
}

// ================================================================
// pwm_fan_get_speed()
// ================================================================
// Retorna la velocidad actual del ventilador (0-100%).
uint8_t pwm_fan_get_speed(void)
{
    return fan_speed_actual;
}

// ================================================================
// PWM del SERVO (cortinas, 50 Hz)
// ================================================================
//
// El servomotor necesita una señal PWM de 50 Hz (período de 20 ms).
//
// ¿Cómo funciona un servo?
//   - La posición del servo se determina por el ancho del pulso:
//     0°  -> pulso de ~1 ms  (duty ~819  con 13 bits)
//     90° -> pulso de ~1.5 ms (duty ~1229)
//     180°-> pulso de ~2 ms  (duty ~1638)
//   - En este código, convertimos 0-100% a 0-180° linealmente
//     y luego a duty usando interpolación lineal entre los
//     valores de calibración.
//
// Resolución de 14 bits:
//   Con 14 bits tenemos 16384 valores posibles (0-16383).
//   A 50 Hz: 16384 / 20ms = ~819 cuentas/ms
//   1 ms = 819 cuentas, 2 ms = 1638 cuentas (valores de duty típicos)
//   Usamos valores calibrados: SERVO_DUTY_0_DEG y SERVO_DUTY_180_DEG

void pwm_servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_PWM_TIMER,      // Timer 1
        .freq_hz         = SERVO_PWM_FREQ_HZ,    // 50 Hz
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = SERVO_PWM_RESOLUTION, // 13 bits
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = SERVO_PWM_CHANNEL,     // Canal 1
        .timer_sel      = SERVO_PWM_TIMER,
        .gpio_num       = PIN_SERVO_PWM,         // GPIO11
        .duty           = SERVO_DUTY_90_DEG,     // Posición inicial: 90°
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

// ================================================================
// pwm_servo_set_position()
// ================================================================
// Mueve el servo a una posición expresada como porcentaje.
//
// Parámetros:
//   porcentaje: 0% = 0° (cerrado), 100% = 180° (abierto)
//
// Cálculo del duty:
//   1. Convertir porcentaje a ángulo: angulo = (porcentaje * 180) / 100
//   2. Interpolar duty entre 0° y 180°:
//      duty = DUTY_0 + (angulo * (DUTY_180 - DUTY_0)) / 180
//
//   Ejemplo: 50% -> angulo = 90° -> duty ~1229 (mitad)
void pwm_servo_set_position(uint8_t porcentaje)
{
    if (porcentaje > 100) porcentaje = 100;

    uint32_t angulo = (porcentaje * 180) / 100;
    uint32_t duty = SERVO_DUTY_0_DEG +
                    (angulo * (SERVO_DUTY_180_DEG - SERVO_DUTY_0_DEG)) / 180;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL));
}
