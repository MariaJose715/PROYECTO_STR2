#ifndef PWM_HANDLER_H
#define PWM_HANDLER_H

#include <stdint.h>

// ============================================================
// MANEJADOR PWM (usando LEDC del ESP32)
// Controla: Ventilador (alta frecuencia) y Servo (50 Hz)
// ============================================================

// ============================================================
// CONFIGURACIÓN DEL VENTILADOR (PWM de alta frecuencia)
// ============================================================
#define FAN_PWM_TIMER      LEDC_TIMER_0
#define FAN_PWM_CHANNEL    LEDC_CHANNEL_0
#define FAN_PWM_FREQ_HZ    25000       // 25 kHz (frecuencia audible evitada)
#define FAN_PWM_RESOLUTION LEDC_TIMER_8_BIT  // Resolución 8-bit (0-255)

// ============================================================
// CONFIGURACIÓN DEL SERVO (PWM de 50 Hz)
// ============================================================
#define SERVO_PWM_TIMER      LEDC_TIMER_1
#define SERVO_PWM_CHANNEL    LEDC_CHANNEL_1
#define SERVO_PWM_FREQ_HZ    50          // 50 Hz (período de 20 ms)
#define SERVO_PWM_RESOLUTION LEDC_TIMER_14_BIT // Resolución 14-bit (0-16383)

// Mapeo de ángulo del servo a duty cycle
// Para servo SG90: 0° = 1ms, 90° = 1.5ms, 180° = 2ms
// Con 14-bit y 50Hz: período = 16384 ticks = 20ms
// 1ms = 819 ticks, 1.5ms = 1229 ticks, 2ms = 1638 ticks
#define SERVO_DUTY_0_DEG    819     // ~1 ms  -> 0°
#define SERVO_DUTY_90_DEG   1229    // ~1.5 ms -> 90°
#define SERVO_DUTY_180_DEG  1638    // ~2 ms  -> 180°

// ============================================================
// PROTOTIPOS
// ============================================================

// Inicializa el PWM para el ventilador
void pwm_fan_init(void);

// Establece la velocidad del ventilador (0% a 100%)
// La velocidad se mapea linealmente al duty cycle del PWM
void pwm_fan_set_speed(uint8_t porcentaje);

// Obtiene la velocidad actual del ventilador (0-100%)
uint8_t pwm_fan_get_speed(void);

// Inicializa el PWM para el servomotor
void pwm_servo_init(void);

// Establece la apertura de cortina (0% = cerrado, 100% = completamente abierto)
// El porcentaje se mapea a un ángulo de 0° a 180° en el servo
void pwm_servo_set_position(uint8_t porcentaje);

#endif // PWM_HANDLER_H
