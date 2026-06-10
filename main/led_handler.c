#include "led_handler.h"
#include "gpio_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================
// IMPLEMENTACIÓN - LED RGB (Iluminación ambiental)
// ============================================================

/**
 * led_rgb_init()
 * Inicializa los pines del LED RGB como salidas y los apaga.
 */
void led_rgb_init(void)
{
    // Los pines ya fueron configurados como salida en gpio_init_all()
    // Aquí simplemente aseguramos que inicien apagados (LOW)
    gpio_set_level(PIN_LED_R, 0);
    gpio_set_level(PIN_LED_G, 0);
    gpio_set_level(PIN_LED_B, 0);
}

/**
 * led_rgb_set()
 * Establece el color del LED RGB usando PWM por software (bit-banging simple).
 * NOTA: En un sistema real se usarían canales LEDC (PWM por hardware).
 *       Aquí usamos GPIO digital con delays para simular PWM de forma didáctica.
 *
 * Para un mejor rendimiento, reemplazar con LEDC (como en pwm_handler).
 *
 * color: puntero a estructura rgb_color_t con R, G, B (0-255) y brillo (0-100).
 */
void led_rgb_set(const rgb_color_t *color)
{
    // Escalamos los valores de 0-255 a 0-100 para el ciclo de trabajo
    // y aplicamos el brillo como factor multiplicativo
    int r_pwm = (color->r * color->brightness) / 255;
    int g_pwm = (color->g * color->brightness) / 255;
    int b_pwm = (color->b * color->brightness) / 255;

    // En una implementación real con LEDC, haríamos:
    // ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, r_pwm * 255 / 100, 0);
    // ledc_set_duty_and_update(...); // para cada canal

    // Por simplicidad didáctica, usamos GPIO digital:
    // Si el valor PWM > 50% del ciclo, encendemos; si no, apagamos.
    // (Esto es una simplificación; el PWM real requiere temporización)
    gpio_set_level(PIN_LED_R, (r_pwm > 50) ? 1 : 0);
    gpio_set_level(PIN_LED_G, (g_pwm > 50) ? 1 : 0);
    gpio_set_level(PIN_LED_B, (b_pwm > 50) ? 1 : 0);
}

/**
 * led_rgb_off()
 * Apaga completamente el LED RGB.
 */
void led_rgb_off(void)
{
    gpio_set_level(PIN_LED_R, 0);
    gpio_set_level(PIN_LED_G, 0);
    gpio_set_level(PIN_LED_B, 0);
}

// ============================================================
// IMPLEMENTACIÓN - LED DE ALARMA (Rojo)
// ============================================================

/**
 * led_alarma_init()
 * Inicializa el pin del LED de alarma como salida y lo apaga.
 */
void led_alarma_init(void)
{
    gpio_set_level(PIN_LED_ALARMA, 0);
}

/**
 * led_alarma_set()
 * Enciende o apaga el LED de alarma.
 * encendido: true = encender, false = apagar.
 */
void led_alarma_set(bool encendido)
{
    gpio_set_level(PIN_LED_ALARMA, encendido ? 1 : 0);
}

/**
 * led_alarma_parpadeo_task()
 * Tarea FreeRTOS para hacer parpadear el LED de alarma a 1 Hz.
 * Debe ser creada con xTaskCreate() y ejecutarse cuando T > T_max.
 *
 * Esta tarea se puede crear al iniciar la alarma y eliminarse al detenerla.
 *
 * param: no utilizado (puede ser NULL).
 */
void led_alarma_parpadeo_task(void *param)
{
    while (1)
    {
        // Encender LED por 1 segundo
        gpio_set_level(PIN_LED_ALARMA, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Apagar LED por 1 segundo
        gpio_set_level(PIN_LED_ALARMA, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
