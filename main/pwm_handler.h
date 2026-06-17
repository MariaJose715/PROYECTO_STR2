/**
 * pwm_handler.h - PWM PARA VENTILADOR Y SERVO
 *
 * Timers LEDC en ESP32-C6:
 *   Timer 0 -> Ventilador (25 kHz, 8-bit)
 *   Timer 1 -> Servo       (50 Hz, 14-bit)
 *   Timer 2 -> LED RGB     (1 kHz, 8-bit) en led_handler.c
 *
 * El ventilador usa 25 kHz para evitar ruido audible.
 * El servo usa 50 Hz (estándar) con 14 bits de resolución.
 */

#ifndef PWM_HANDLER_H
#define PWM_HANDLER_H

#include <stdint.h>

// --- Ventilador (alta frecuencia) ---
#define FAN_PWM_TIMER      LEDC_TIMER_0
#define FAN_PWM_CHANNEL    LEDC_CHANNEL_0
#define FAN_PWM_FREQ_HZ    25000
#define FAN_PWM_RESOLUTION LEDC_TIMER_8_BIT

// --- Servo (50 Hz estándar) ---
#define SERVO_PWM_TIMER      LEDC_TIMER_1
#define SERVO_PWM_CHANNEL    LEDC_CHANNEL_1
#define SERVO_PWM_FREQ_HZ    50

// Con 14-bit y 50 Hz: período = 16384 ticks = 20 ms
//   1 ms = 819 ticks -> 0°
//   1.5 ms = 1229 ticks -> 90°
//   2 ms = 1638 ticks -> 180°
#define SERVO_PWM_RESOLUTION LEDC_TIMER_14_BIT
#define SERVO_DUTY_0_DEG    819
#define SERVO_DUTY_90_DEG   1229
#define SERVO_DUTY_180_DEG  1638

void pwm_fan_init(void);
void pwm_fan_set_speed(uint8_t porcentaje);
uint8_t pwm_fan_get_speed(void);

void pwm_servo_init(void);
void pwm_servo_set_position(uint8_t porcentaje);

#endif
