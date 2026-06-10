#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// MANEJADOR DE LEDS
// Controla el LED RGB (iluminación ambiental) y el LED de alarma
// ============================================================

// Estructura para definir un color RGB
typedef struct {
    uint8_t r;          // Rojo   (0 - 255)
    uint8_t g;          // Verde  (0 - 255)
    uint8_t b;          // Azul   (0 - 255)
    uint8_t brightness; // Brillo (0 - 100%)
} rgb_color_t;

// ============================================================
// PROTOTIPOS - LED RGB
// ============================================================

// Inicializa los pines PWM para el LED RGB
void led_rgb_init(void);

// Establece el color y brillo del LED RGB
// color: estructura con valores R, G, B (0-255) y brillo (0-100)
void led_rgb_set(const rgb_color_t *color);

// Apaga el LED RGB por completo
void led_rgb_off(void);

// ============================================================
// PROTOTIPOS - LED DE ALARMA
// ============================================================

// Inicializa el pin del LED de alarma
void led_alarma_init(void);

// Enciende o apaga el LED de alarma
void led_alarma_set(bool encendido);

// Tarea para parpadeo 1Hz (1s on / 1s off)
// Debe ejecutarse en un bucle infinito dentro de una tarea FreeRTOS
void led_alarma_parpadeo_task(void *param);

#endif // LED_HANDLER_H
