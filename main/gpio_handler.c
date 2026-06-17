/**
 * ================================================================
 * gpio_handler.c - INICIALIZACIÓN DE PINES GPIO
 * ================================================================
 *
 * Este archivo configura todos los pines GPIO del ESP32-C6
 * usando la API gpio_config_t de ESP-IDF.
 *
 * Organización de pines:
 *
 *   SALIDAS:
 *     PIN_LED_R      = GPIO5   -> LED RGB (Rojo)
 *     PIN_LED_G      = GPIO6   -> LED RGB (Verde)
 *     PIN_LED_B      = GPIO7   -> LED RGB (Azul)
 *     PIN_LED_ALARMA = GPIO8   -> LED Alarma (rojo)
 *     PIN_FAN_PWM    = GPIO10  -> Ventilador PWM
 *     PIN_SERVO_PWM  = GPIO11  -> Servo cortina PWM
 *
 *   ENTRADAS:
 *     PIN_TEMP_SENSOR = GPIO4  -> NTC (ADC)
 *     PIN_BOOT_BUTTON = GPIO0  -> Botón BOOT (con pull-up interno)
 *
 *   UART:
 *     PIN_UART_TX = GPIO16     -> TX del conversor USB-UART
 *     PIN_UART_RX = GPIO17     -> RX del conversor USB-UART
 *
 * ¿Por qué se usa gpio_config_t en lugar de gpio_set_direction()?
 *   gpio_config_t permite configurar varios pines a la vez con una
 *   máscara de bits (pin_bit_mask). Es más eficiente y es el patrón
 *   recomendado en los ejemplos oficiales de ESP-IDF.
 *
 * ================================================================
 */

#include "gpio_handler.h"
#include "driver/gpio.h"
#include "esp_err.h"

// ================================================================
// gpio_init_all()
// ================================================================
// Inicializa todos los pines GPIO del sistema.
//
// La configuración se hace en 2 grupos:
//   1. Salidas: LEDs RGB, LED alarma, PWM ventilador, PWM servo
//   2. Entradas: Sensor NTC (ADC), botón BOOT
//
// Cada grupo usa gpio_config() con una máscara de bits que indica
// qué pines pertenecen a ese grupo.
//
// pin_bit_mask: es un entero de 64 bits donde cada bit representa
// un GPIO. Ejemplo: (1ULL << GPIO5) | (1ULL << GPIO6) incluye
// los pines 5 y 6.
void gpio_init_all(void)
{
    // ---- 1. Configurar pines como SALIDA ----
    // Estos pines pueden recibir señales PWM (LEDC) o digitales (GPIO)
    gpio_config_t io_conf_salida = {
        .intr_type    = GPIO_INTR_DISABLE,  // Sin interrupción
        .mode         = GPIO_MODE_OUTPUT,   // Modo salida
        .pin_bit_mask = (1ULL << PIN_LED_R) |
                        (1ULL << PIN_LED_G) |
                        (1ULL << PIN_LED_B) |
                        (1ULL << PIN_LED_ALARMA) |
                        (1ULL << PIN_FAN_PWM) |
                        (1ULL << PIN_SERVO_PWM),
        .pull_down_en = 0,  // Sin pull-down
        .pull_up_en   = 0,  // Sin pull-up
    };
    gpio_config(&io_conf_salida);

    // ---- 2. Configurar pines como ENTRADA ----
    // El pin del sensor NTC se usa como entrada analógica (ADC),
    // el botón BOOT como entrada digital
    gpio_config_t io_conf_entrada = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,    // Modo entrada
        .pin_bit_mask = (1ULL << PIN_TEMP_SENSOR) |
                        (1ULL << PIN_BOOT_BUTTON),
        .pull_down_en = 0,
        .pull_up_en   = 0,
    };
    gpio_config(&io_conf_entrada);

    // El botón BOOT necesita pull-up interno activado individualmente
    // porque en la placa ESP32-C6-DevKit no tiene pull-up externo
    gpio_set_pull_mode(PIN_BOOT_BUTTON, GPIO_PULLUP_ONLY);

    // Asegurar que todas las salidas inicien en LOW (apagadas)
    gpio_set_level(PIN_LED_ALARMA, 0);
}
