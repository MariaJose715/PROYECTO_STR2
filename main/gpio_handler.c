#include "gpio_handler.h"
#include "driver/gpio.h"
#include "esp_err.h"

/**
 * gpio_init_all()
 * Inicializa todos los pines GPIO del sistema usando la estructura
 * gpio_config_t (patrón oficial de los ejemplos de ESP-IDF).
 *
 * Se configura primero el grupo de salidas, luego el de entradas,
 * usando la máscara de bits (pin_bit_mask) para cada grupo.
 */
void gpio_init_all(void)
{
    // ---- 1. Configurar pines como SALIDA ----
    gpio_config_t io_conf_salida = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_LED_R) |
                        (1ULL << PIN_LED_G) |
                        (1ULL << PIN_LED_B) |
                        (1ULL << PIN_LED_ALARMA) |
                        (1ULL << PIN_FAN_PWM) |
                        (1ULL << PIN_SERVO_PWM),
        .pull_down_en = 0,
        .pull_up_en   = 0,
    };
    gpio_config(&io_conf_salida);

    // ---- 2. Configurar pines como ENTRADA ----
    gpio_config_t io_conf_entrada = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_TEMP_SENSOR) |
                        (1ULL << PIN_BOOT_BUTTON),
        .pull_down_en = 0,
        .pull_up_en   = 0,
    };
    gpio_config(&io_conf_entrada);

    // El botón BOOT necesita pull-up interno activado individualmente
    gpio_set_pull_mode(PIN_BOOT_BUTTON, GPIO_PULLUP_ONLY);

    // Asegurar que todas las salidas inicien en LOW
    gpio_set_level(PIN_LED_ALARMA, 0);
}
