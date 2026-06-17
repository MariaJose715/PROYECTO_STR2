/**
 * gpio_handler.h - DEFINICIÓN DE PINES GPIO DEL SISTEMA
 *
 * Asignación de pines para el ESP32-C6:
 *   - GPIO0:  Botón BOOT
 *   - GPIO4:  ADC1_CH4 -> Sensor NTC
 *   - GPIO5:  LED RGB Rojo
 *   - GPIO6:  LED RGB Verde
 *   - GPIO7:  LED RGB Azul
 *   - GPIO8:  LED Alarma
 *   - GPIO10: PWM Ventilador
 *   - GPIO11: PWM Servo cortina
 *   - GPIO16: UART0 TX
 *   - GPIO17: UART0 RX
 */

#ifndef GPIO_HANDLER_H
#define GPIO_HANDLER_H

#include "driver/gpio.h"

// --- LED RGB: 3 canales PWM independientes ---
#define PIN_LED_R           GPIO_NUM_5
#define PIN_LED_G           GPIO_NUM_6
#define PIN_LED_B           GPIO_NUM_7

// --- LED Alarma: GPIO digital simple ---
#define PIN_LED_ALARMA      GPIO_NUM_8

// --- Ventilador: PWM 25 kHz ---
#define PIN_FAN_PWM         GPIO_NUM_10

// --- Servo cortinas: PWM 50 Hz ---
#define PIN_SERVO_PWM       GPIO_NUM_11

// --- Sensor NTC: ADC1_CH4 ---
#define PIN_TEMP_SENSOR     GPIO_NUM_4

// --- UART0: depuración serie ---
#define PIN_UART_TX         GPIO_NUM_16
#define PIN_UART_RX         GPIO_NUM_17

// --- Botón BOOT ---
#define PIN_BOOT_BUTTON     GPIO_NUM_0

void gpio_init_all(void);

#endif
