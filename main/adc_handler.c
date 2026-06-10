#include "adc_handler.h"
#include "gpio_handler.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_err.h"

// Manejadores del ADC (API oneshot - patrón del ejemplo oficial)
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

/**
 * adc_temp_init()
 * Inicializa el ADC siguiendo el patrón del ejemplo oficial oneshot_read.
 * Incluye calibración con fallback (curve_fitting -> line_fitting).
 */
void adc_temp_init(void)
{
    // ---- 1. Inicializar la unidad ADC1 en modo oneshot ----
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // ---- 2. Configurar el canal del sensor ----
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_TEMP_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_TEMP_CHANNEL, &chan_cfg));

    // ---- 3. Configurar calibración (con fallback, como en el ejemplo oficial) ----
    bool calibrada = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_TEMP_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
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
        esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle);
        if (ret == ESP_OK) calibrada = true;
    }
#endif

    (void)calibrada; // Prevenir warning si no hay esquema de calibración
}

/**
 * adc_temp_read_raw()
 * Lee el valor crudo del ADC (0 - 4095 para 12 bits).
 */
int adc_temp_read_raw(void)
{
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return -1;
    return raw_value;
}

/**
 * adc_temp_read_celsius()
 * Lee la temperatura desde el sensor LM35 y la convierte a °C.
 * LM35: 10 mV = 1°C.
 */
float adc_temp_read_celsius(void)
{
    int raw_value = 0;
    int voltaje_mv = 0;

    // Leer valor crudo del ADC
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return 0.0f;

    // Convertir a voltaje usando calibración (si disponible)
    if (cali_handle != NULL)
    {
        ret = adc_cali_raw_to_voltage(cali_handle, raw_value, &voltaje_mv);
    }

    // Si no hay calibración o falló, estimar: V = (raw / 4095) * 3300 mV
    if (ret != ESP_OK || cali_handle == NULL)
    {
        voltaje_mv = (raw_value * 3300) / 4095;
    }

    return (float)voltaje_mv / 10.0f;
}
