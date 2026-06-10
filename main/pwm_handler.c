#include "pwm_handler.h"
#include "gpio_handler.h"
#include "driver/ledc.h"
#include "esp_err.h"

// ============================================================
// PWM del VENTILADOR (alta frecuencia ~25 kHz)
// ============================================================

static uint8_t fan_speed_actual = 0;  // Velocidad actual del ventilador

void pwm_fan_init(void)
{
    // Configurar temporizador LEDC (patrón oficial ejemplo ledc_basic)
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = FAN_PWM_TIMER,
        .freq_hz         = FAN_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = FAN_PWM_RESOLUTION,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // Configurar canal LEDC
    ledc_channel_config_t channel_config = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = FAN_PWM_CHANNEL,
        .timer_sel      = FAN_PWM_TIMER,
        .gpio_num       = PIN_FAN_PWM,
        .duty           = 0,
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void pwm_fan_set_speed(uint8_t porcentaje)
{
    if (porcentaje > 100) porcentaje = 100;
    uint32_t duty = (porcentaje * 255) / 100;
    fan_speed_actual = porcentaje;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_PWM_CHANNEL));
}

uint8_t pwm_fan_get_speed(void)
{
    return fan_speed_actual;
}

// ============================================================
// PWM del SERVO (cortinas, 50 Hz)
// ============================================================

void pwm_servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_PWM_TIMER,
        .freq_hz         = SERVO_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = SERVO_PWM_RESOLUTION,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = SERVO_PWM_CHANNEL,
        .timer_sel      = SERVO_PWM_TIMER,
        .gpio_num       = PIN_SERVO_PWM,
        .duty           = SERVO_DUTY_90_DEG,
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void pwm_servo_set_position(uint8_t porcentaje)
{
    if (porcentaje > 100) porcentaje = 100;

    uint32_t angulo = (porcentaje * 180) / 100;
    uint32_t duty = SERVO_DUTY_0_DEG +
                    (angulo * (SERVO_DUTY_180_DEG - SERVO_DUTY_0_DEG)) / 180;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL));
}
