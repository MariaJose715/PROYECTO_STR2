#include "uart_handler.h"
#include "gpio_handler.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

// Configuración del puerto UART para depuración
#define UART_PORT_NUM       UART_NUM_0
#define UART_BUF_SIZE       1024
#define UART_RX_BUF_SIZE    (UART_BUF_SIZE * 2)
#define UART_TX_BUF_SIZE    (UART_BUF_SIZE * 2)

/**
 * uart_init()
 * Inicializa la UART0 siguiendo el patrón oficial de los ejemplos de ESP-IDF.
 *
 * Orden correcto: 1) config parámetros  2) config pines  3) instalar driver
 * Esto coincide con el ejemplo uart_echo de ESP-IDF.
 */
void uart_init(void)
{
    // Configurar parámetros de la UART (.source_clk agregado como en ejemplo oficial)
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Aplicar la configuración
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    // Configurar los pines TX y RX (RTS/CTS = UART_PIN_NO_CHANGE)
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, PIN_UART_TX, PIN_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Instalar el driver con buffers
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM,
                                        UART_RX_BUF_SIZE,
                                        UART_TX_BUF_SIZE,
                                        0, NULL, 0));
}

/**
 * uart_send_msg()
 * Envía un mensaje formateado por UART (similar a printf).
 */
void uart_send_msg(const char *formato, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, formato);
    vsnprintf(buffer, sizeof(buffer), formato, args);
    va_end(args);

    uart_write_bytes(UART_PORT_NUM, buffer, strlen(buffer));
    uart_write_bytes(UART_PORT_NUM, "\r\n", 2);
}

/**
 * uart_read_line()
 * Lee una línea de texto desde UART (hasta encontrar '\n').
 */
int uart_read_line(char *buffer, int max_len, int timeout_ms)
{
    int len = 0;
    memset(buffer, 0, max_len);

    while (len < max_len - 1)
    {
        char c;
        int read_len = uart_read_bytes(UART_PORT_NUM, &c, 1, pdMS_TO_TICKS(timeout_ms));
        if (read_len <= 0) break;

        if (c == '\n' || c == '\r')
        {
            if (len > 0 && buffer[len - 1] == '\r')
                buffer[len - 1] = '\0';
            break;
        }
        buffer[len++] = c;
    }
    buffer[len] = '\0';
    return len;
}
