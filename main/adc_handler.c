#include "adc_handler.h"
#include "gpio_handler.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_err.h"
#include <math.h>

void adc_temp_init(system_context_t *ctx)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &ctx->adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_TEMP_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ctx->adc_handle, ADC_TEMP_CHANNEL, &chan_cfg));

    bool calibrada = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_TEMP_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &ctx->cali_handle);
    if (ret == ESP_OK) calibrada = true;
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrada)
    {
        adc_cali_line_fitting_config_t cali_cfg = {
            .unit_id  = ADC_UNIT_1,
            .atten    = ADC_TEMP_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &ctx->cali_handle);
        if (ret == ESP_OK) calibrada = true;
    }
#endif

    (void)calibrada;
}

int adc_temp_read_raw(system_context_t *ctx)
{
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(ctx->adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return -1;
    return raw_value;
}

float adc_temp_read_celsius(system_context_t *ctx)
{
    int raw_value = 0;
    int voltaje_mv = 0;

    esp_err_t ret = adc_oneshot_read(ctx->adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return 25.0f;

    if (ctx->cali_handle != NULL)
    {
        ret = adc_cali_raw_to_voltage(ctx->cali_handle, raw_value, &voltaje_mv);
    }

    if (ret != ESP_OK || ctx->cali_handle == NULL)
    {
        voltaje_mv = (raw_value * 3300) / 4095;
    }

    const float VCC_MV = 3300.0f;
    const float R_FIJO = 10000.0f;
    const float R0     = 10000.0f;
    const float B      = 3950.0f;
    const float T0     = 298.15f;

    if (voltaje_mv <= 0 || voltaje_mv >= VCC_MV) return 25.0f;

    float r_ntc = R_FIJO * (VCC_MV / (float)voltaje_mv - 1.0f);
    if (r_ntc <= 0.0f) return 25.0f;

    float razon = r_ntc / R0;
    float log_val = logf(razon);
    if (!isfinite(log_val)) return 25.0f;

    float inv_t = 1.0f / T0 + (1.0f / B) * log_val;
    if (inv_t <= 0.0f) return 25.0f;

    float temp_k = 1.0f / inv_t;
    float temp_c = temp_k - 273.15f;

    if (temp_c < -10.0f) return -10.0f;
    if (temp_c > 125.0f) return 125.0f;

    return temp_c;
}
