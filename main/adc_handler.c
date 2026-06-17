/**
 * ================================================================
 * adc_handler.c - LECTURA DE TEMPERATURA CON NTC
 * ================================================================
 *
 * Este archivo maneja la lectura del sensor de temperatura NTC
 * (Negative Temperature Coefficient) usando el ADC oneshot del
 * ESP32-C6.
 *
 * ¿Cómo funciona un NTC?
 *   - Es una resistencia cuyo valor disminuye al aumentar la temperatura
 *   - Se conecta en configuración de divisor de voltaje con una
 *     resistencia fija (R_FIJO = 10kΩ)
 *   - El voltaje en el punto medio del divisor varía con la temperatura
 *   - El ADC mide ese voltaje y lo convertimos a temperatura usando
 *     la ecuación Beta del fabricante
 *
 * Circuito:
 *   3.3V ─── NTC (10k @25°C) ─── ADC_GPIO4 ─── R_FIJO (10kΩ) ─── GND
 *
 *                +--- Va medido por ADC ---+
 *
 * Fórmulas:
 *   1. R_ntc = R_fijo * (Vsupply / Vadc - 1)
 *   2. 1/T = 1/T0 + (1/B) * ln(R_ntc / R0)  ← Ecuación Beta
 *
 *   Donde:
 *     T0 = 298.15K (25°C)
 *     R0 = 10000Ω (resistencia del NTC a 25°C)
 *     B  = 3950 (coeficiente Beta, dado por el fabricante)
 *     R_FIJO = 10000Ω (resistencia fija del divisor)
 *
 * Pines:
 *   PIN_TEMP_SENSOR = GPIO4 (ADC1_CHANNEL_4 en ESP32-C6)
 * ================================================================
 */

#include "adc_handler.h"
#include "gpio_handler.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_err.h"
#include <math.h>

// Manejadores del ADC (API oneshot - patrón del ejemplo oficial)
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// ================================================================
// adc_temp_init()
// ================================================================
// Inicializa el ADC siguiendo el patrón del ejemplo oficial
// oneshot_read de ESP-IDF.
//
// Pasos:
//   1. Crear una unidad ADC1 en modo oneshot (lectura única)
//   2. Configurar el canal del sensor NTC con atenuación de 11 dB
//      (rango de medición: 0-3.3V → ~150-2450 mV mapeado a 0-4095)
//   3. Configurar calibración (curve_fitting o line_fitting)
//
// ¿Por qué calibración?
//   El ADC del ESP32 no es perfectamente lineal. La calibración
//   corrige la no-linealidad usando valores de fábrica almacenados
//   en eFuse. Si no hay calibración disponible, usamos un fallback
//   simple: Voltaje = (raw * 3300) / 4095
void adc_temp_init(void)
{
    // ---- 1. Inicializar la unidad ADC1 en modo oneshot ----
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // ---- 2. Configurar el canal del sensor ----
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_TEMP_ATTEN,           // ADC_ATTEN_DB_11 (rango completo)
        .bitwidth = ADC_BITWIDTH_DEFAULT,     // 12 bits (0-4095)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_TEMP_CHANNEL, &chan_cfg));

    // ---- 3. Configurar calibración (con fallback, como en el ejemplo oficial) ----
    bool calibrada = false;

    // Intentar calibración curve_fitting primero (más precisa)
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_TEMP_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret == ESP_OK) calibrada = true;
#endif

    // Fallback: calibración line_fitting (menos precisa pero más compatible)
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

// ================================================================
// adc_temp_read_raw()
// ================================================================
// Lee el valor crudo del ADC (0-4095 para 12 bits).
//
// Retorna: valor crudo del ADC, o -1 si hay error de lectura.
int adc_temp_read_raw(void)
{
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return -1;
    return raw_value;
}

// ================================================================
// adc_temp_read_celsius()
// ================================================================
// Lee la temperatura desde el NTC usando la ecuación Beta.
//
// Algoritmo:
//   1. Leer valor crudo del ADC (0-4095)
//   2. Convertir a voltaje usando calibración (o fallback lineal)
//   3. Calcular R_ntc: R_ntc = R_FIJO * (VCC / V_adc - 1)
//   4. Calcular temperatura con ecuación Beta:
//      1/T = 1/T0 + (1/B) * ln(R_ntc / R0)
//   5. Convertir Kelvin a Celsius
//
// Protecciones:
//   - Si el voltaje es 0 o igual a VCC, retorna 25°C (fuera de rango)
//   - Si R_ntc <= 0, retorna 25°C
//   - Si log() produce NaN o Inf, retorna 25°C
//   - Satura el resultado entre -10°C y +125°C
//
// Retorna: temperatura en grados Celsius (float)
float adc_temp_read_celsius(void)
{
    int raw_value = 0;
    int voltaje_mv = 0;

    // Leer valor crudo del ADC
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_TEMP_CHANNEL, &raw_value);
    if (ret != ESP_OK) return 25.0f;

    // Convertir a voltaje usando calibración (si disponible)
    if (cali_handle != NULL)
    {
        ret = adc_cali_raw_to_voltage(cali_handle, raw_value, &voltaje_mv);
    }

    // Fallback: estimar voltaje si no hay calibración
    if (ret != ESP_OK || cali_handle == NULL)
    {
        voltaje_mv = (raw_value * 3300) / 4095;
    }

    // ---- Convertir voltaje NTC a temperatura usando ecuación Beta ----
    //
    // Constantes del sensor NTC:
    //   R_FIJO = 10000Ω (resistencia fija del divisor de voltaje)
    //   R0     = 10000Ω (resistencia del NTC a 25°C — dato del fabricante)
    //   B      = 3950   (coeficiente Beta — dato del fabricante)
    //   T0     = 298.15K (25°C en Kelvin)
    //
    // IMPORTANTE: Estas constantes deben coincidir con el NTC real.
    // Si el NTC es de 10k (B=3950), usar R0=10000 y R_FIJO=10000.
    // Si el NTC es de 100k, las constantes cambian.

    const float VCC_MV = 3300.0f;       // Voltaje de alimentación (3.3V)
    const float R_FIJO = 10000.0f;      // Resistencia fija del divisor (10kΩ)
    const float R0     = 10000.0f;      // Resistencia del NTC a 25°C (10kΩ)
    const float B      = 3950.0f;       // Coeficiente Beta del NTC
    const float T0     = 298.15f;       // 25°C en Kelvin

    // Protección: voltaje fuera de rango
    if (voltaje_mv <= 0 || voltaje_mv >= VCC_MV) return 25.0f;

    // Calcular resistencia del NTC a partir del divisor de voltaje
    float r_ntc = R_FIJO * (VCC_MV / (float)voltaje_mv - 1.0f);
    if (r_ntc <= 0.0f) return 25.0f;

    // Aplicar ecuación Beta
    float razon = r_ntc / R0;
    float log_val = logf(razon);

    // Protección: logf() puede producir NaN o Inf si razon <= 0
    if (!isfinite(log_val)) return 25.0f;

    float inv_t = 1.0f / T0 + (1.0f / B) * log_val;
    if (inv_t <= 0.0f) return 25.0f;

    float temp_k = 1.0f / inv_t;
    float temp_c = temp_k - 273.15f;

    // Saturación del resultado a límites físicos razonables
    if (temp_c < -10.0f) return -10.0f;
    if (temp_c > 125.0f) return 125.0f;

    return temp_c;
}
