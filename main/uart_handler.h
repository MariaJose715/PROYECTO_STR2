/**
 * uart_handler.h - MANEJO DE UART PARA DEPURACIÓN
 *
 * La UART0 (GPIO16=TX, GPIO17=RX) se usa para:
 *   - Mensajes de depuración durante inicialización
 *   - Log de eventos del sistema (WiFi, OTA, alarmas)
 *   - Posible interfaz de comandos vía terminal serie
 *
 * Baud rate: 115200 (estándar)
 */

#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <stdint.h>

#define UART_BAUD_RATE      115200

void uart_init(void);
void uart_send_msg(const char *formato, ...);
int  uart_read_line(char *buffer, int max_len, int timeout_ms);

#endif
