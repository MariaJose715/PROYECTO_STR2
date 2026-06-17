/**
 * ================================================================
 * led_handler.c - CONTROL DE LEDS
 * ================================================================
 *
 * Este archivo maneja dos tipos de LEDs:
 *
 *   1. LED RGB (iluminación ambiental):
 *      - Usa 3 canales PWM independientes del periférico LEDC
 *      - Un timer compartido (timer 2) con 3 canales (2, 3, 4)
 *      - Frecuencia: 1 kHz (suficiente para LEDs, sin parpadeo visible)
 *      - Resolución: 8 bits (0-255) para cada canal
 *      - El brillo se combina con el color: valor_final = (color * brillo) / 100
 *
 *   2. LED de alarma (rojo, parpadeante):
 *      - GPIO digital simple (encendido/apagado)
 *      - Parpadeo a 1 Hz mediante una tarea FreeRTOS (creada/eliminada
 *        dinámicamente desde main.c cuando hay/cesa la alarma)
 *
 *   Pines usados (definidos en gpio_handler.h):
 *     LED_R     = GPIO5
 *     LED_G     = GPIO6
 *     LED_B     = GPIO7
 *     LED_ALARMA = GPIO8
 * ================================================================
 */

#include "led_handler.h"
#include "gpio_handler.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ================================================================
// CONFIGURACIÓN DEL PWM PARA EL LED RGB
// ================================================================
//
// El ESP32-C6 tiene varios timers LEDC. Distribuimos así:
//   Timer 0 -> Ventilador (25 kHz)  [en pwm_handler.c]
//   Timer 1 -> Servo      (50 Hz)   [en pwm_handler.c]
//   Timer 2 -> LED RGB    (1 kHz)   [AQUÍ - este archivo]
//
// Los 3 canales RGB comparten el mismo timer pero tienen
// distintos pines de salida. Esto ahorra recursos.

#define LED_RGB_TIMER       LEDC_TIMER_2     // Timer para LEDs RGB
#define LED_RGB_SPEED_MODE  LEDC_LOW_SPEED_MODE  // ESP32-C6 solo soporta LOW_SPEED
#define LED_RGB_RESOLUTION  LEDC_TIMER_8_BIT // 8 bits = valores 0-255
#define LED_RGB_FREQ_HZ     1000            // 1 kHz (evita parpadeo visible)

// Canales PWM asignados a cada color del LED RGB
#define LEDR_CHANNEL    LEDC_CHANNEL_2  // Canal 2 -> LED Rojo   (GPIO5)
#define LEDG_CHANNEL    LEDC_CHANNEL_3  // Canal 3 -> LED Verde  (GPIO6)
#define LEDB_CHANNEL    LEDC_CHANNEL_4  // Canal 4 -> LED Azul   (GPIO7)

// ================================================================
// led_rgb_init()
// ================================================================
// Inicializa el timer y los 3 canales PWM para el LED RGB.
//
// ¿Por qué usamos LEDC en lugar de GPIO normal?
//   - GPIO normal solo puede encender/apagar (HIGH/LOW)
//   - LEDC permite controlar el brillo con PWM (modulación por ancho de pulso)
//   - Con 8 bits de resolución, tenemos 256 niveles de brillo por canal
//   - Combinando 3 canales (R+G+B) podemos generar 256^3 = 16.7 millones de colores
//
// ESP_ERROR_CHECK: si la configuración falla, el programa se detiene
// (útil para depuración, no queremos que el sistema funcione con PWM mal configurado).
void led_rgb_init(void)
{
    // --- Configurar el timer compartido para los 3 canales ---
    ledc_timer_config_t timer = {
        .speed_mode      = LED_RGB_SPEED_MODE,
        .timer_num       = LED_RGB_TIMER,
        .freq_hz         = LED_RGB_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,   // Elige el reloj automáticamente
        .duty_resolution = LED_RGB_RESOLUTION,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // --- Configurar cada canal PWM (Rojo, Verde, Azul) ---
    // Cada canal se asocia a: un timer, un GPIO y un duty inicial (0 = apagado)
    ledc_channel_config_t ch_r = {
        .speed_mode = LED_RGB_SPEED_MODE, .channel = LEDR_CHANNEL,
        .timer_sel  = LED_RGB_TIMER, .gpio_num = PIN_LED_R,
        .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_r));

    ledc_channel_config_t ch_g = {
        .speed_mode = LED_RGB_SPEED_MODE, .channel = LEDG_CHANNEL,
        .timer_sel  = LED_RGB_TIMER, .gpio_num = PIN_LED_G,
        .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_g));

    ledc_channel_config_t ch_b = {
        .speed_mode = LED_RGB_SPEED_MODE, .channel = LEDB_CHANNEL,
        .timer_sel  = LED_RGB_TIMER, .gpio_num = PIN_LED_B,
        .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_b));
}

// ================================================================
// led_rgb_set()
// ================================================================
// Establece el color y brillo del LED RGB.
//
// ¿Cómo se calcula el duty de cada canal?
//   El usuario elige:
//     - Un color RGB (r, g, b) donde cada valor va de 0 a 255
//     - Un brillo global de 0 a 100%
//
//   El duty final para cada canal es:
//     duty = (componente_de_color * brillo_global) / 100
//
//   Ejemplo: color rojo puro (255, 0, 0) con brillo 50%:
//     duty_R = (255 * 50) / 100 = 127  (≈ 50% de 255)
//     duty_G = (0 * 50) / 100 = 0
//     duty_B = (0 * 50) / 100 = 0
//
// Parámetros:
//   color: puntero a estructura rgb_color_t con los valores
//         color->r = rojo   (0-255)
//         color->g = verde  (0-255)
//         color->b = azul   (0-255)
//         color->brightness = brillo global (0-100)
void led_rgb_set(const rgb_color_t *color)
{
    // Calcular duty para cada canal combinando color + brillo
    uint32_t duty_r = ((uint32_t)color->r * color->brightness) / 100;
    uint32_t duty_g = ((uint32_t)color->g * color->brightness) / 100;
    uint32_t duty_b = ((uint32_t)color->b * color->brightness) / 100;

    // Saturación: si algún duty excede 255 (por error de redondeo), lo limitamos
    if (duty_r > 255) duty_r = 255;
    if (duty_g > 255) duty_g = 255;
    if (duty_b > 255) duty_b = 255;

    // Aplicar los nuevos duties a los canales PWM
    // ledc_set_duty() solo actualiza un registro interno
    // ledc_update_duty() hace efectivo el cambio en el hardware
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL, duty_r);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL);

    ledc_set_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL, duty_g);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL);

    ledc_set_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL, duty_b);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL);
}

// ================================================================
// led_rgb_off()
// ================================================================
// Apaga el LED RGB por completo (pone los 3 canales a duty 0).
void led_rgb_off(void)
{
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL, 0);
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL, 0);
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL, 0);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL);
}

// ================================================================
// FUNCIONES DEL LED DE ALARMA (GPIO digital)
// ================================================================
// El LED de alarma es simple: se enciende o se apaga con GPIO digital.
// No necesita PWM porque solo queremos dos estados:
//   - Encendido fijo (cuando se inicializa)
//   - Parpadeo 1 Hz (cuando hay alarma - manejado por una tarea FreeRTOS)

void led_alarma_init(void)
{
    // Asegurar que el LED de alarma comience apagado
    gpio_set_level(PIN_LED_ALARMA, 0);
}

void led_alarma_set(bool encendido)
{
    // true = encender, false = apagar
    gpio_set_level(PIN_LED_ALARMA, encendido ? 1 : 0);
}

// ================================================================
// TAREA FreeRTOS: led_alarma_parpadeo_task()
// ================================================================
//
// Esta función es el cuerpo de una tarea FreeRTOS que se crea
// en main.c cuando hay alarma de temperatura.
//
// ¿Qué hace?
//   1. Enciende el LED
//   2. Espera 1 segundo (vTaskDelay)
//   3. Apaga el LED
//   4. Espera 1 segundo
//   5. Repite desde el paso 1 (while(1))
//
// ¿Cómo se detiene?
//   Cuando la temperatura vuelve a la normalidad, main.c elimina
//   esta tarea con vTaskDelete(). La tarea no sabe que la van a
//   eliminar, simplemente se detiene en medio del ciclo.
//
// Parámetro:
//   param: no se usa (puede ser NULL)
void led_alarma_parpadeo_task(void *param)
{
    while (1)
    {
        gpio_set_level(PIN_LED_ALARMA, 1);   // Encender LED
        vTaskDelay(pdMS_TO_TICKS(1000));      // Esperar 1 segundo

        gpio_set_level(PIN_LED_ALARMA, 0);   // Apagar LED
        vTaskDelay(pdMS_TO_TICKS(1000));      // Esperar 1 segundo
    }
}
