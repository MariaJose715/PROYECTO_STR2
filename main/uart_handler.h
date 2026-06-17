#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <stdint.h>

// ============================================================
// MANEJADOR UART (Comunicación Serial)
// Se utiliza para enviar mensajes de depuración a la consola
// ============================================================

#define UART_BAUD_RATE      115200  // Velocidad de transmisión estándar

// ============================================================
// PROTOTIPOS
// ============================================================

// Inicializa la UART para depuración
void uart_init(void);

// Envía un mensaje formateado por UART (similar a printf)
void uart_send_msg(const char *formato, ...);

// Lee una línea de texto desde UART (bloqueante, con timeout)
int uart_read_line(char *buffer, int max_len, int timeout_ms);

#endif // UART_HANDLER_H
