#ifndef GPIO_HANDLER_H
#define GPIO_HANDLER_H

#include "driver/gpio.h"

// ============================================================
// DEFINICIÓN DE PINES GPIO
// Cada pin del ESP32 se asigna a un componente físico del sistema
// ============================================================

// --- LED RGB (control de iluminación ambiental) ---
#define PIN_LED_R           GPIO_NUM_5     // Canal Rojo   (PWM)
#define PIN_LED_G           GPIO_NUM_6     // Canal Verde  (PWM)
#define PIN_LED_B           GPIO_NUM_7     // Canal Azul   (PWM)

// --- LED de alarma (Rojo, encendido/apagado simple) ---
#define PIN_LED_ALARMA      GPIO_NUM_8     // Se enciende cuando T > T_max

// --- Ventilador (PWM para velocidad) ---
#define PIN_FAN_PWM         GPIO_NUM_10    // Salida PWM para el ventilador

// --- Servomotor (control de cortinas) ---
#define PIN_SERVO_PWM       GPIO_NUM_11    // Salida PWM para el servo

// --- Sensor de temperatura (entrada analógica) ---
#define PIN_TEMP_SENSOR     GPIO_NUM_4     // ADC1_CH4 (solo entrada)

// --- UART (depuración por serial) ---
#define PIN_UART_TX         GPIO_NUM_16    // TX de UART0
#define PIN_UART_RX         GPIO_NUM_17    // RX de UART0

// --- Botón de reinicio / modo configuración ---
#define PIN_BOOT_BUTTON     GPIO_NUM_0     // Botón BOOT del ESP32

// ============================================================
// PROTOTIPOS
// ============================================================

// Inicializa todos los pines GPIO del sistema
void gpio_init_all(void);

#endif // GPIO_HANDLER_H
