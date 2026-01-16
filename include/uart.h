#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_tx(const uint8_t *data, int len);
int uart_rx_nonblocking(uint8_t *buf, int maxlen);
void uart_tx_blocking(const char *s);

#endif // UART_H
