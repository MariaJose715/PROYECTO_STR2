#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include "system_context.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#define ADC_TEMP_CHANNEL    ADC_CHANNEL_4
#define ADC_TEMP_ATTEN      ADC_ATTEN_DB_12

void adc_temp_init(system_context_t *ctx);
int adc_temp_read_raw(system_context_t *ctx);
float adc_temp_read_celsius(system_context_t *ctx);

#endif
