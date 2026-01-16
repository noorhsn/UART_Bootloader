# UART Bootloader — Hardware TODOs (STM32L496)

This TODO lists the UART/USART hardware configuration, register writes, and related MCU settings required to implement the bare-metal UART bootloader on the STM32L496 series.

This document intentionally uses descriptive register/bit names and code-like pseudocode with placeholders (e.g. `USARTx`, `RCC_APB1ENR_USARTxEN`, `PIN_AF_UARTx`) instead of hard-coded addresses. Before implementing, we'll need the STM32L4 Reference Manual (RM0351 or equivalent) and the device datasheet for exact register names/addresses and flash page size constants. See "Additional docs needed" at the end.

---

## 1) Choose UART peripheral & pins

- Pick a UART/USART peripheral (e.g. `USART1`, `USART2`, `LPUART1`) and the TX/RX pins.
- Confirm these pins support the appropriate AF (alternate function) for the chosen UART on the STM32L496 package.
- TODO: Record chosen peripheral and pins here:
  - Peripheral: ____ (e.g. USART1)
  - TX pin: ____ (e.g. PA9)
  - RX pin: ____ (e.g. PA10)
  - AF value: `PIN_AF_UARTx` (per datasheet)

Reason: pin choice affects RCC (APB1/APB2), AF mapping and NVIC IRQ number.

---

## 2) RCC / Clock enable

Before configuring GPIO or USART registers, enable the peripheral & GPIO clocks.

Example pseudocode:

```c
// enable GPIO clock for TX/RX ports
RCC->AHB2ENR |= (1 << GPIOX_EN_BIT); // e.g. AHB2ENR GPIOAEN

// enable USART clock: USART1 is usually on APB2, USART2 on APB1
RCC->APB2ENR |= (1 << USART1_EN_BIT); // or APB1ENR for APB1 peripherals

// small delay to let clocks start (readback)
volatile uint32_t tmp = RCC->APB2ENR;
(void)tmp;
```

Notes:
- Use the correct RCC register (`APB1ENR1`, `APB1ENR2`, `APB2ENR`) depending on peripheral and MCU family revision (check RM0351 / reference manual).

---

## 3) GPIO configuration (TX = alternate push-pull, RX = alternate input)

Steps for each pin (TX and RX):

- Set MODER to Alternate Function (MODER = 10).
- Set OTYPER = push-pull (0) for TX.
- Set OSPEEDR = high or very high for baud stability (choose according to signal frequency).
- Set PUPDR = pull-up for RX (recommended) or as design requires.
- Set AFR[ ] = AF value for chosen USART (PIN_AF_UARTx).

Pseudocode:

```c
// Configure TX pin
GPIOX->MODER &= ~(3 << (TX_PIN*2));
GPIOX->MODER |=  (2 << (TX_PIN*2)); // AF
GPIOX->OTYPER &= ~(1 << TX_PIN);    // push-pull
GPIOX->OSPEEDR |=  (2 << (TX_PIN*2)); // high speed
GPIOX->PUPDR &= ~(3 << (TX_PIN*2));
GPIOX->PUPDR |=  (1 << (TX_PIN*2)); // pull-up (optional)
GPIOX->AFR[TX_PIN/8] &= ~(0xF << ((TX_PIN%8)*4));
GPIOX->AFR[TX_PIN/8] |= (PIN_AF_UARTx << ((TX_PIN%8)*4));

// Configure RX pin similarly, but input type behavior
```

---

## 4) USART configuration (115200, 8N1, RX interrupt)

High-level sequence (pseudocode):

1. Disable USART (UE = 0) while configuring
2. Configure word length, parity, stop bits in `USARTx->CR1/CR2`
3. Configure baud rate `USARTx->BRR` from `PCLK` (APB clock feeding USART)
4. Configure hardware flow control or other CR3 features (none for simple 8N1)
5. Enable RXNE interrupt (RXNEIE) and optionally error interrupts (ORE, FE)
6. Enable NVIC for `USARTx_IRQn` and set priority
7. Enable TE and RE
8. Enable UE (USART enable)

Example pseudocode:

```c
// 1. Disable
USARTx->CR1 &= ~USART_CR1_UE;

// 2. Word length / parity / stop bits (8N1)
USARTx->CR1 &= ~(USART_CR1_M | USART_CR1_PCE); // 8-bit, parity disabled
USARTx->CR2 &= ~(USART_CR2_STOP); // 1 stop bit

// 3. Baud: BRR calculation (oversampling by 16 assumed)
// USARTDIV = PCLK / baud
// For integer & fractional BRR fields: BRR = mantissa<<4 | fraction
uint32_t pclk = get_apb_clock_freq_for_usart(USARTx);
uint32_t usartdiv = (pclk + (baud/2)) / baud; // approximate
USARTx->BRR = usartdiv; // or compute mantissa+frac precisely per RM

// 4. CR3 default: disable hardware flow control
USARTx->CR3 &= ~(USART_CR3_CTSE | USART_CR3_RTSE);

// 5. Enable RXNE interrupt
USARTx->CR1 |= USART_CR1_RXNEIE;

// 6. NVIC
NVIC_EnableIRQ(USARTx_IRQn);
NVIC_SetPriority(USARTx_IRQn, NVIC_PRIORITY_BOOTLOADER);

// 7. Enable transmitter and receiver
USARTx->CR1 |= (USART_CR1_TE | USART_CR1_RE);

// 8. Enable USART
USARTx->CR1 |= USART_CR1_UE;
```

Important details for baud rate (do not skip):

- You must use the correct PCLK frequency for the selected USART peripheral (APB1 or APB2 clock). If APB prescaler != 1, note that many STM32 parts double the PCLK for timers only; for USART use the actual bus clock.
- Baud calculation for oversampling by 16 (default):

  USARTDIV = PCLK / Baud
  DIV_Mantissa = int(USARTDIV)
  DIV_Fraction = round((USARTDIV - DIV_Mantissa) * 16)
  BRR = (DIV_Mantissa << 4) | (DIV_Fraction & 0xF)

- If using oversampling by 8 (OVR8 bit = 1), fraction scaling is 8 instead of 16.

---

## 5) RX interrupt handler & buffering

- Implement an IRQ handler for `USARTx_IRQn`.
- In the handler check `USARTx->ISR` (or `SR`) flags for `RXNE` and errors (ORE, FE, NE).
- On `RXNE`, read `USARTx->RDR` into a circular RX buffer and advance write index.
- On errors, clear error flags (read RDR/ISR/ICR depending on RM) and optionally request retransmit.
- Provide APIs for the main loop to `uart_rx_nonblocking()` which read bytes from the circular buffer without blocking.

Pseudocode IRQ:

```c
void USARTx_IRQHandler(void) {
    uint32_t isr = USARTx->ISR; // or SR for older naming
    if (isr & USART_ISR_RXNE) {
        uint8_t b = (uint8_t)(USARTx->RDR & 0xFF);
        rx_buffer[rx_wi++] = b; // wrap index
    }
    if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        // clear flags according to RM (read RDR/ISR or write ICR)
        uint32_t icr = USART_ICR_*; // write to ICR if available
    }
}
```

Notes:
- Ensure IRQ priority is correct (bootloader should be responsive; choose a high priority but leave room for critical interrupts)
- Protect indices when accessed from main context and IRQ (use atomic reads/writes or disable IRQ briefly while manipulating indices if necessary).

---

## 6) Optional: DMA for RX (stretch goal)

- If you plan to use DMA to receive large firmware streams with minimal CPU overhead, configure the DMA channel for peripheral-to-memory circular or normal mode.
- Steps:
  - Enable DMA clock
  - Configure DMA stream/channel: peripheral address = &USARTx->RDR, memory address = buffer, direction = P2M
  - Configure transfer size, data width (byte), enable transfer complete interrupt
  - Configure USART CR3 DMAR bit (enable DMA for receiver)
  - Start DMA

Notes:
- DMA saves CPU time but requires careful handling for partial chunks and verifying data integrity per-chunk.

---

## 7) Flash erase / program safety checks (summary)

IMPORTANT: Do not erase or program the bootloader's flash region. Only erase and program starting at `APP_START`.

High-level flow (hardware-specific):

1. Unlock flash control registers (write key sequence to FLASH_KEYR or PEKEYR depending on MCU family)
2. Clear error flags
3. For each page in the application region that will be programmed:
   - Issue page erase (set PER/PSIZE and start)
   - Wait for completion and check for errors
4. Program data in aligned half-words/words/blocks as required by MCU (e.g. 16-bit, 32-bit, or 64-bit programming)
5. After each programming operation, read back and verify memory
6. Lock flash control registers

Pseudocode:

```c
flash_unlock();
for (addr = APP_START; addr < APP_START + fw_size; addr += page_size) {
    flash_erase_page(addr);
}

for (addr = APP_START; written < fw_size; addr += write_unit) {
    flash_program_unit(addr, &data[offset], write_unit);
    if (flash_verify(addr, &data[offset], write_unit) != 0) error();
}
flash_lock();
```

Notes that require exact RM lookup:
- Unlock sequence registers/names (FLASH->KEYR etc.) and required key values
- Page size and alignment constraints (page, double-word, or row size)
- Programming unit: half-word, word, double-word — exact API depends on FLASH controller revision

---

## 8) Integrity check (QuickSort + XOR)

- After full firmware is written and verified per-page, read entire firmware region into RAM buffer (must fit into memory limits — e.g. chunk if necessary).
- Run `quicksort()` (already implemented) on the buffer and compute adjacent XOR checksum.
- Request the host's checksum and compare.
- On mismatch: respond `CHECKSUM_FAIL` and do NOT jump to application.

Memory note: For large firmware (e.g. > available RAM), the bootloader must either:
- Use an external checksum that does not require sorting the entire firmware in RAM, or
- Implement an on-flash algorithm that can sort/stream using limited RAM (more complex).

---

## 9) Jump to application (vector remap & MSP)

Sequence (hardware):

1. Verify application stack pointer and reset vector are valid (not 0xFFFFFFFF or erased).
2. De-initialize peripherals used by bootloader (disable IRQs, timers, USART, DMA)
3. Disable systick (if used) and other interrupts
4. Set vector table offset register (SCB->VTOR) to `APP_START`
   ```c
   SCB->VTOR = APP_START;
   __DSB(); __ISB();
   ```
5. Set MSP to the first word at APP_START
   ```c
   __set_MSP(*(uint32_t *)APP_START);
   ```
6. Jump to reset handler (second word at APP_START)
   ```c
   pfunc reset_handler = (pfunc)*(uint32_t *)(APP_START + 4);
   reset_handler();
   ```

Notes:
- Ensure that caches (if present) and prefetch are handling flash mapping correctly; invalidate caches if necessary.

---

## 10) IRQ priorities and responsiveness

- Set NVIC priority for USART IRQ to a value that keeps bootloader responsive but allows higher-priority system faults to be handled.
- Consider SysTick usage: ensure it doesn't interfere with timing-critical UART transfer.

---

## 11) Timeouts and error handling (protocol)

- Implement receive timeouts for packet/command phases.
- If a chunk is corrupted (verify fails) request retransmit or abort the transfer.
- Implement retransmit window and limits to avoid infinite loops.

---

## 12) Build-time and linker considerations

- Linker script must reserve the bootloader region and place bootloader vector table at `BOOTLOADER_START` (0x08000000) — already present, but verify exact flash size and alignment for STM32L496.
- Mark `APP_START` as the start of the application and ensure bootloader does not overlap.

---

## 13) Test checklist

- Hardware smoke test: configure UART pins and verify loopback (TX→RX physically) at 115200.
- Protocol test: successful transfer of small test app (e.g., blinky) and verify jump.
- Negative test: send corrupted chunk and verify bootloader sends CHECKSUM_FAIL and does not jump.
- Power-loss test: interrupt transfer mid-write and test whether bootloader recovers or rejects corrupted app.

---

## Additional documents / information needed

To implement the exact register writes and constants I need the following (please provide or confirm):

1. STM32L4 Reference Manual (RM0351 or the specific RM for L4 series) — contains peripheral register descriptions (USART, RCC, FLASH controller, NVIC behavior).
2. STM32L496 Datasheet (device-specific) — package pin mapping, AF mappings, flash size and flash page/row sizes, VDD ranges, and electrical timing.
3. STM32L4 Flash programming manual or AN (application note) if available — contains recommended flash programming sequences and constraints.
4. Which exact UART peripheral and which physical pins you want to use for the bootloader (if you have a board schematic or board name, include it).

If you confirm the above or supply the RM reference (I can work from the attached datasheet but the reference manual is necessary for register-level programming), I will draft the exact C register writes and small driver code for the UART init, IRQ handler, NVIC setup, and flash programming sequences for `USARTx` and the STM32L496.

---

## Quick action items I can take next (pick any):

- [ ] Implement full `uart.c` with register-level config for a chosen USART (I will need the RM and chosen pins).
- [ ] Implement `flash.c` hardware flash sequences using RM0351 specifics (needs RM/AN and page size)
- [ ] Add NVIC and startup vector wiring (`startup.s`) and update linker script with exact flash sizes.
- [ ] Create unit test harness to test QuickSort speed for a 32KB sample on host.

Please tell me which UART peripheral and pins you prefer, and whether you can provide the STM32L4 Reference Manual (RM0351) or want me to proceed from the provided STM32L496 datasheet only.
