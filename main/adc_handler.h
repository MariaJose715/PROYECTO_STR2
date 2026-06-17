/**
 * adc_handler.h - LECTURA DE TEMPERATURA CON NTC VÍA ADC
 *
 * Circuito: divisor de voltaje con NTC 10k (B=3950)
 *   3.3V ─── NTC ─── ADC_GPIO4 ─── 10kΩ ─── GND
 *
 * Canal ADC: ADC1_CHANNEL_4 (GPIO4 del ESP32-C6)
 * Atenuación: 12 dB (rango de entrada 0-3.3V)
 */

#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include <stdint.h>

#define ADC_TEMP_CHANNEL    ADC_CHANNEL_4
#define ADC_TEMP_ATTEN      ADC_ATTEN_DB_12

void adc_temp_init(void);
int  adc_temp_read_raw(void);
float adc_temp_read_celsius(void);

#endif
