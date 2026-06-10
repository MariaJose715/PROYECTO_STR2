#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include <stdint.h>

// ============================================================
// MANEJADOR ADC (Conversión Analógico-Digital)
// Lee la temperatura ambiente desde un sensor analógico (ej: LM35)
// Usa la API moderna de ADC (oneshot) compatible con ESP32-C6
// ============================================================

#define ADC_TEMP_CHANNEL    ADC_CHANNEL_4   // GPIO4 = ADC1_CH4 (ESP32-C6)
#define ADC_TEMP_ATTEN      ADC_ATTEN_DB_12 // Atenuación 12dB (0-3.3V)

// ============================================================
// PROTOTIPOS
// ============================================================

// Inicializa el ADC para el sensor de temperatura
void adc_temp_init(void);

// Lee el valor crudo del ADC (0 - 4095 para 12 bits)
int adc_temp_read_raw(void);

// Lee y convierte la temperatura a grados Celsius
// Asume sensor LM35: 10 mV por °C
float adc_temp_read_celsius(void);

#endif // ADC_HANDLER_H
