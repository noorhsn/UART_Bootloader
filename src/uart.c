#include "uart.h"
#include <stdint.h>
#include <string.h>
#include "config.h"

#ifdef USE_SIM
#include <stdio.h>
#include <unistd.h>

void uart_init(void) { /* no-op for sim */ }

void uart_tx(const uint8_t *data, int len) {
    fwrite(data, 1, len, stdout);
    fflush(stdout);
}

int uart_rx_nonblocking(uint8_t *buf, int maxlen) {
    /* Simple non-blocking read from stdin is tricky; for sim tests we block briefly */
    int n = fread(buf, 1, maxlen, stdin);
    return n;
}

void uart_tx_blocking(const char *s) { fputs(s, stdout); fflush(stdout); }

#else
/* Hardware-specific UART driver skeleton
   Fill in register addresses for your target MCU (STM32L4 series).
   This file intentionally keeps a minimal, portable interface while
   leaving MCU register details as TODOs so you can adapt safely.
*/

void uart_init(void) {
    // TODO: Configure GPIO pins for TX/RX
    // TODO: Configure USART registers for 115200 8N1, enable RXNE interrupt
}

void uart_tx(const uint8_t *data, int len) {
    // TODO: transmit bytes (polling) using USART->TDR register
    (void)data; (void)len;
}

int uart_rx_nonblocking(uint8_t *buf, int maxlen) {
    // TODO: copy from RX circular buffer filled by IRQ handler
    (void)buf; (void)maxlen; return 0;
}

void uart_tx_blocking(const char *s) {
    // TODO: send NUL-terminated string via uart_tx
    (void)s;
}

#endif
