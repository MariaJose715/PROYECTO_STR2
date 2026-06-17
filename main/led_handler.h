/**
 * led_handler.h - CONTROL DE LEDS
 *
 * LED RGB: 3 canales LEDC PWM (timer 2)
 *   - Resolución 8-bit por canal (0-255)
 *   - Frecuencia 1 kHz (sin parpadeo)
 *
 * LED Alarma: GPIO digital
 *   - Parpadeo 1 Hz en tarea FreeRTOS separada
 */

#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness; // 0-100%
} rgb_color_t;

void led_rgb_init(void);
void led_rgb_set(const rgb_color_t *color);
void led_rgb_off(void);

void led_alarma_init(void);
void led_alarma_set(bool encendido);
void led_alarma_parpadeo_task(void *param);

#endif
