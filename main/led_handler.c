#include "led_handler.h"
#include "gpio_handler.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_RGB_TIMER       LEDC_TIMER_2
#define LED_RGB_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define LED_RGB_RESOLUTION  LEDC_TIMER_8_BIT
#define LED_RGB_FREQ_HZ     1000

#define LEDR_CHANNEL    LEDC_CHANNEL_2
#define LEDG_CHANNEL    LEDC_CHANNEL_3
#define LEDB_CHANNEL    LEDC_CHANNEL_4

void led_rgb_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LED_RGB_SPEED_MODE,
        .timer_num       = LED_RGB_TIMER,
        .freq_hz         = LED_RGB_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = LED_RGB_RESOLUTION,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

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

void led_rgb_set(const rgb_color_t *color)
{
    uint32_t duty_r = ((uint32_t)color->r * color->brightness) / 100;
    uint32_t duty_g = ((uint32_t)color->g * color->brightness) / 100;
    uint32_t duty_b = ((uint32_t)color->b * color->brightness) / 100;

    if (duty_r > 255) duty_r = 255;
    if (duty_g > 255) duty_g = 255;
    if (duty_b > 255) duty_b = 255;

    ledc_set_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL, duty_r);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL);

    ledc_set_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL, duty_g);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL);

    ledc_set_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL, duty_b);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL);
}

void led_rgb_off(void)
{
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL, 0);
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL, 0);
    ledc_set_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL, 0);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDR_CHANNEL);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDG_CHANNEL);
    ledc_update_duty(LED_RGB_SPEED_MODE, LEDB_CHANNEL);
}

void led_alarma_init(void)
{
    gpio_set_level(PIN_LED_ALARMA, 0);
}

void led_alarma_set(bool encendido)
{
    gpio_set_level(PIN_LED_ALARMA, encendido ? 1 : 0);
}

void led_alarma_parpadeo_task(void *param)
{
    while (1)
    {
        gpio_set_level(PIN_LED_ALARMA, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(PIN_LED_ALARMA, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
