#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USART1 @ 115200, 8N1, oversampling by 16.
// pclk2_hz must be the APB2 clock in Hz (here: 16 MHz).
void uart_init_usart1(uint32_t pclk2_hz);

// Blocking transmit (single buffer)
void uart_tx_blocking(const uint8_t *buf, size_t len);

// Transmit using DMA (non-blocking). Returns 0 on success.
int uart_tx_dma(const uint8_t *buf, size_t len);

// Start DMA-based RX into provided buffer of given size.
// Returns 0 on success, non-zero on error.
int uart_start_dma_rx(uint8_t *buf, size_t buf_size);

// Stop DMA RX.
void uart_stop_dma_rx(void);

// Get number of bytes currently received into the RX buffer (using DMA NDTR).
size_t uart_dma_rx_count(void);

// Weak callback invoked when an IDLE + DMA sample indicates end-of-frame.
// Override in application.
void uart_rx_frame_received(size_t len);

// Weak callback invoked when TX DMA completes. Override in application.
void uart_tx_done_callback(void);

#ifdef __cplusplus
}
#endif

#endif // UART_H
