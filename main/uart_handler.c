/**
 * ================================================================
 * uart_handler.c - COMUNICACIÓN UART (Depuración)
 * ================================================================
 *
 * Este archivo maneja la comunicación serie por UART0 para
 * mensajes de depuración y diagnóstico.
 *
 * El ESP32-C6 tiene 3 UARTs. Usamos la UART0 porque está
 * conectada al conversor USB-UART integrado en la placa de
 * desarrollo (GPIO16=TX, GPIO17=RX).
 *
 * ¿Para qué sirve la UART?
 *   - Imprimir mensajes de depuración (estado del sistema,
 *     errores, eventos WiFi, etc.)
 *   - Recibir comandos del usuario (si se implementa un
 *     intérprete de comandos por UART)
 *   - Ver el log de inicialización cuando el ESP32 se conecta
 *     al puerto serie del PC
 *
 * Pines (definidos en gpio_handler.h):
 *   PIN_UART_TX = GPIO16
 *   PIN_UART_RX = GPIO17
 *   Baud rate: 115200 (configurable en gpio_handler.h)
 * ================================================================
 */

#include "uart_handler.h"
#include "gpio_handler.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

// Configuración del puerto UART para depuración
#define UART_PORT_NUM       UART_NUM_0   // UART0 (conectada al USB)
#define UART_BUF_SIZE       1024         // Tamaño base de buffer
#define UART_RX_BUF_SIZE    (UART_BUF_SIZE * 2)  // Buffer de recepción
#define UART_TX_BUF_SIZE    (UART_BUF_SIZE * 2)  // Buffer de transmisión

// ================================================================
// uart_init()
// ================================================================
// Inicializa la UART0 siguiendo el patrón oficial de los ejemplos
// de ESP-IDF (ejemplo: uart_echo).
//
// El orden de inicialización es importante:
//   1. uart_param_config()   -> configura baud rate, bits de datos, etc.
//   2. uart_set_pin()        -> asigna los pines TX/RX
//   3. uart_driver_install() -> instala el driver con buffers
//
// source_clk = UART_SCLK_DEFAULT: permite que el driver elija
// automáticamente la fuente de reloj adecuada para el ESP32-C6.
void uart_init(void)
{
    // Configurar parámetros de la UART
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,          // 115200 bps
        .data_bits  = UART_DATA_8_BITS,         // 8 bits de datos
        .parity     = UART_PARITY_DISABLE,      // Sin paridad
        .stop_bits  = UART_STOP_BITS_1,         // 1 bit de parada
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE, // Sin control de flujo
        .source_clk = UART_SCLK_DEFAULT,        // Reloj automático
    };

    // Aplicar la configuración al puerto UART0
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    // Configurar los pines TX y RX
    // RTS/CTS = UART_PIN_NO_CHANGE (no se usan)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, PIN_UART_TX, PIN_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Instalar el driver con buffers de 2KB para TX y RX
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM,
                                        UART_RX_BUF_SIZE,  // Buffer RX
                                        UART_TX_BUF_SIZE,  // Buffer TX
                                        0, NULL, 0));       // Sin cola de eventos
}

// ================================================================
// uart_send_msg()
// ================================================================
// Envía un mensaje formateado por UART (similar a printf).
//
// Usa lista variable de argumentos (va_list) para aceptar
// cadenas con formato como printf.
//
// Ejemplo:
//   uart_send_msg("Temperatura: %.1f°C", 25.3);
//   -> Imprime: "Temperatura: 25.3°C\r\n"
//
// Parámetros:
//   formato: cadena con formato (igual que printf)
//   ...: argumentos variables
void uart_send_msg(const char *formato, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, formato);
    vsnprintf(buffer, sizeof(buffer), formato, args);
    va_end(args);

    uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
    uart_write_bytes(UART_PORT_NUM, "\r\n", 2);  // Agregar salto de línea
}

// ================================================================
// uart_read_line()
// ================================================================
// Lee una línea de texto desde UART (hasta encontrar '\n' o '\r').
//
// Útil para recibir comandos del usuario por el puerto serie.
//
// Parámetros:
//   buffer: donde se almacena la línea leída
//   max_len: tamaño máximo del buffer
//   timeout_ms: tiempo máximo de espera por carácter (ms)
//
// Retorna:
//   Número de caracteres leídos (sin contar el nulo final)
//   0 si no se recibió nada (timeout)
int uart_read_line(char *buffer, int max_len, int timeout_ms)
{
    int len = 0;
    memset(buffer, 0, max_len);

    while (len < max_len - 1)
    {
        char c;
        int read_len = uart_read_bytes(UART_PORT_NUM, &c, 1, pdMS_TO_TICKS(timeout_ms));
        if (read_len <= 0) break;  // Timeout o error

        if (c == '\n' || c == '\r')
        {
            // Si hay un \r antes del final, lo quitamos
            if (len > 0 && buffer[len - 1] == '\r')
                buffer[len - 1] = '\0';
            break;
        }
        buffer[len++] = c;
    }
    buffer[len] = '\0';
    return len;
}
