/*
  DMA-based USART1 TX/RX implementation (STM32L4 family).

  Uses:
    - DMA1_Channel1 for USART1_TX
    - DMA1_Channel2 for USART1_RX

  Based on RM0432 mapping for STM32L4 series DMA request channel indices.
  Confirm channel mapping for your exact package in RM0432 / device datasheet.
*/

#include "uart.h"

// Simulation stubs
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "stm32l4xx.h" // CMSIS device header

/* === DMA channel mapping for STM32L496 (per RM0432 DMA request mapping) ===
   - USART1 TX  -> DMA1 Channel1
   - USART1 RX  -> DMA1 Channel2

   IRQ and ISR/IFCR flags follow the CMSIS names:
     DMA1_Channel1_IRQn, DMA1_Channel2_IRQn
     DMA_ISR_TCIF1, DMA_IFCR_CTCIF1, DMA_ISR_TCIF2, DMA_IFCR_CTCIF2

   Please confirm mapping from RM0432 for your exact package.
*/
#define USART1_DMA_TX_CHANNEL      DMA1_Channel1
#define USART1_DMA_TX_IRQn         DMA1_Channel1_IRQn
#define USART1_DMA_TX_ISR_TC_FLAG  DMA_ISR_TCIF1
#define USART1_DMA_TX_IFCR_CTCIF   DMA_IFCR_CTCIF1

#define USART1_DMA_RX_CHANNEL      DMA1_Channel2
#define USART1_DMA_RX_IRQn         DMA1_Channel2_IRQn
#define USART1_DMA_RX_ISR_TC_FLAG  DMA_ISR_TCIF2
#define USART1_DMA_RX_IFCR_CTCIF   DMA_IFCR_CTCIF2

/* RX buffer pointer (set by uart_start_dma_rx) */
static uint8_t *dma_rx_buffer = NULL;
static size_t dma_rx_buf_size = 0;

/* TX DMA state */
static const uint8_t *tx_dma_buf = NULL;
static size_t tx_dma_len = 0;

static uint32_t compute_usart_brr_overs16(uint32_t pclk_hz, uint32_t baud)
{
    uint64_t scaled = ((uint64_t)pclk_hz * 16ULL + (baud / 2)) / (uint64_t)baud;
    uint32_t mant = (uint32_t)(scaled / 16U);
    uint32_t frac = (uint32_t)(scaled - ((uint64_t)mant * 16ULL));
    return (mant << 4) | (frac & 0xFU);
}

void uart_init_usart1(uint32_t pclk2_hz)
{
    /* Enable GPIOA & USART1 clocks */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* Configure PA9 = AF7 TX, PA10 = AF7 RX */
    GPIOA->MODER &= ~((3U << (9*2)) | (3U << (10*2)));
    GPIOA->MODER |=  ((2U << (9*2)) | (2U << (10*2)));
    GPIOA->AFR[1] &= ~((0xFU << ((9-8)*4)) | (0xFU << ((10-8)*4)));
    GPIOA->AFR[1] |=  ((7U << ((9-8)*4)) | (7U << ((10-8)*4)));
    GPIOA->OTYPER &= ~((1U<<9) | (1U<<10));
    GPIOA->OSPEEDR &= ~((3U << (9*2)) | (3U << (10*2)));
    GPIOA->OSPEEDR |=  ((2U << (9*2)) | (2U << (10*2)));
    GPIOA->PUPDR &= ~((3U << (9*2)) | (3U << (10*2)));
    GPIOA->PUPDR |=  ((0U << (9*2)) | (1U << (10*2)));

    /* Disable USART while configuring */
    USART1->CR1 &= ~USART_CR1_UE;

    /* 8N1 */
    USART1->CR1 &= ~(USART_CR1_M | USART_CR1_PCE);
    USART1->CR2 &= ~(USART_CR2_STOP);

    /* Baud rate */
    USART1->BRR = compute_usart_brr_overs16(pclk2_hz, 115200U);

    /* Enable DMA receiver */
    USART1->CR3 |= USART_CR3_DMAR;

    /* Enable IDLE interrupt to detect end-of-frame when using DMA */
    USART1->CR1 |= USART_CR1_IDLEIE;

    /* Enable transmitter and receiver */
    USART1->CR1 |= (USART_CR1_TE | USART_CR1_RE);

    /* NVIC for USART1 */
    NVIC_SetPriority(USART1_IRQn, 5);
    NVIC_EnableIRQ(USART1_IRQn);

    /* Enable USART */
    USART1->CR1 |= USART_CR1_UE;
}

void uart_tx_blocking(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        while (!(USART1->ISR & USART_ISR_TXE)) { }
        USART1->TDR = buf[i] & 0xFFU;
    }
    while (!(USART1->ISR & USART_ISR_TC)) { }
}

/* Start DMA-based TX (non-blocking). Caller must ensure buffer remains valid until callback. */
int uart_tx_dma(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) return -1;

    /* enable DMA1 clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* configure channel 1 for memory-to-peripheral */
    USART1_DMA_TX_CHANNEL->CCR &= ~DMA_CCR_EN;
    while (USART1_DMA_TX_CHANNEL->CCR & DMA_CCR_EN) { }

    USART1_DMA_TX_CHANNEL->CPAR = (uint32_t)&USART1->TDR;
    USART1_DMA_TX_CHANNEL->CMAR = (uint32_t)buf;
    USART1_DMA_TX_CHANNEL->CNDTR = (uint32_t)len;

    /* DIR = 1 (memory->peripheral), MINC, normal mode, priority high */
    USART1_DMA_TX_CHANNEL->CCR = 0;
    USART1_DMA_TX_CHANNEL->CCR |= DMA_CCR_MINC;
    USART1_DMA_TX_CHANNEL->CCR |= DMA_CCR_DIR; /* memory->peripheral */
    /* Clear circular bit (normal transfer) */
    USART1_DMA_TX_CHANNEL->CCR &= ~DMA_CCR_CIRC;
    USART1_DMA_TX_CHANNEL->CCR |= DMA_CCR_TCIE; /* enable transfer complete interrupt */
    USART1_DMA_TX_CHANNEL->CCR |= DMA_CCR_PL_1; /* set priority high */

    /* Clear pending flags for channel1 */
    DMA1->IFCR = USART1_DMA_TX_IFCR_CTCIF;

    /* Enable NVIC for DMA TX channel */
    NVIC_SetPriority(USART1_DMA_TX_IRQn, 5);
    NVIC_EnableIRQ(USART1_DMA_TX_IRQn);

    /* enable the channel */
    tx_dma_buf = buf;
    tx_dma_len = len;
    USART1_DMA_TX_CHANNEL->CCR |= DMA_CCR_EN;

    /* Ensure USART DMAT set (enable DMA transmitter) */
    USART1->CR3 |= USART_CR3_DMAT;

    return 0;
}

int uart_start_dma_rx(uint8_t *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) return -1;

    dma_rx_buffer = buf;
    dma_rx_buf_size = buf_size;

    /* enable DMA1 clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* configure RX channel (peripheral->memory) */
    USART1_DMA_RX_CHANNEL->CCR &= ~DMA_CCR_EN;
    while (USART1_DMA_RX_CHANNEL->CCR & DMA_CCR_EN) { }

    USART1_DMA_RX_CHANNEL->CPAR = (uint32_t)&USART1->RDR;
    USART1_DMA_RX_CHANNEL->CMAR = (uint32_t)buf;
    USART1_DMA_RX_CHANNEL->CNDTR = (uint32_t)buf_size;

    /* DIR=0 (peripheral->memory), MINC, circular mode */
    USART1_DMA_RX_CHANNEL->CCR = 0;
    USART1_DMA_RX_CHANNEL->CCR |= DMA_CCR_MINC;
    USART1_DMA_RX_CHANNEL->CCR &= ~DMA_CCR_DIR;
    USART1_DMA_RX_CHANNEL->CCR |= DMA_CCR_CIRC;
    USART1_DMA_RX_CHANNEL->CCR |= DMA_CCR_TCIE;
    USART1_DMA_RX_CHANNEL->CCR |= DMA_CCR_PL_1;

    /* Clear pending flags for channel2 */
    DMA1->IFCR = USART1_DMA_RX_IFCR_CTCIF;

    /* NVIC for DMA RX */
    NVIC_SetPriority(USART1_DMA_RX_IRQn, 5);
    NVIC_EnableIRQ(USART1_DMA_RX_IRQn);

    /* Enable USART DMAR (done in init but ensure set) */
    USART1->CR3 |= USART_CR3_DMAR;

    /* Enable channel */
    USART1_DMA_RX_CHANNEL->CCR |= DMA_CCR_EN;

    return 0;
}

void uart_stop_dma_rx(void)
{
    if (USART1_DMA_RX_CHANNEL->CCR & DMA_CCR_EN) {
        USART1_DMA_RX_CHANNEL->CCR &= ~DMA_CCR_EN;
        while (USART1_DMA_RX_CHANNEL->CCR & DMA_CCR_EN) { }
    }
    dma_rx_buffer = NULL;
    dma_rx_buf_size = 0;
}

size_t uart_dma_rx_count(void)
{
    if (dma_rx_buffer == NULL) return 0;
    return (size_t)(dma_rx_buf_size - (size_t)USART1_DMA_RX_CHANNEL->CNDTR);
}

/* Weak callbacks */
__attribute__((weak)) void uart_rx_frame_received(size_t len) { (void)len; }
__attribute__((weak)) void uart_tx_done_callback(void) { }

/* USART1 IRQ handler */
void USART1_IRQHandler(void)
{
    uint32_t isr = USART1->ISR;

    if (isr & USART_ISR_IDLE) {
        volatile uint32_t tmp = USART1->ISR;
        (void)tmp;
        (void)USART1->RDR; /* clear IDLE */

        if (dma_rx_buffer != NULL && dma_rx_buf_size > 0) {
            size_t received = dma_rx_buf_size - (size_t)USART1_DMA_RX_CHANNEL->CNDTR;
            uart_rx_frame_received(received);
        }
    }

    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE)) {
        volatile uint32_t tmp = USART1->ISR;
        (void)tmp;
        (void)USART1->RDR;
    }
}

/* DMA1 Channel2 IRQ handler - RX */
void DMA1_Channel2_IRQHandler(void)
{
    /* Check transfer complete flag for channel2 */
    if (DMA1->ISR & USART1_DMA_RX_ISR_TC_FLAG) {
        /* clear transfer complete flag */
        DMA1->IFCR = USART1_DMA_RX_IFCR_CTCIF;
        /* For circular mode we may treat TC as buffer-full event */
        if (dma_rx_buffer != NULL) {
            uart_rx_frame_received(dma_rx_buf_size);
        }
    }
    /* handle transfer error flags if necessary */
}

/* DMA1 Channel1 IRQ handler - TX */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & USART1_DMA_TX_ISR_TC_FLAG) {
        /* clear TX TC flag */
        DMA1->IFCR = USART1_DMA_TX_IFCR_CTCIF;
        /* disable channel (normal mode) */
        USART1_DMA_TX_CHANNEL->CCR &= ~DMA_CCR_EN;
        while (USART1_DMA_TX_CHANNEL->CCR & DMA_CCR_EN) { }
        /* optionally clear DMAT if no concurrent TX activity */
        USART1->CR3 &= ~USART_CR3_DMAT;
        tx_dma_buf = NULL;
        tx_dma_len = 0;
        uart_tx_done_callback();
    }
}