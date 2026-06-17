#ifndef PWM_HANDLER_H
#define PWM_HANDLER_H

#include <stdint.h>
#include "system_context.h"

#define FAN_PWM_TIMER      LEDC_TIMER_0
#define FAN_PWM_CHANNEL    LEDC_CHANNEL_0
#define FAN_PWM_FREQ_HZ    25000
#define FAN_PWM_RESOLUTION LEDC_TIMER_8_BIT

#define SERVO_PWM_TIMER      LEDC_TIMER_1
#define SERVO_PWM_CHANNEL    LEDC_CHANNEL_1
#define SERVO_PWM_FREQ_HZ    50
#define SERVO_PWM_RESOLUTION LEDC_TIMER_14_BIT
#define SERVO_DUTY_0_DEG    819
#define SERVO_DUTY_90_DEG   1229
#define SERVO_DUTY_180_DEG  1638

void pwm_fan_init(void);
void pwm_fan_set_speed(system_context_t *ctx, uint8_t porcentaje);
uint8_t pwm_fan_get_speed(system_context_t *ctx);
void pwm_servo_init(void);
void pwm_servo_set_position(uint8_t porcentaje);

#endif
