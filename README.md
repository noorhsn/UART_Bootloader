# UART Bootloader (STM32L496) — QuickSort Integrity Check

This repository contains a small bare-metal UART bootloader scaffold for the STM32L496 family.
It demonstrates a simple firmware transfer protocol over UART, writing to flash, and a QuickSort-based
integrity check applied on the written firmware before jumping to the application.

Highlights
- Bootloader region: 0x08000000 - 0x08003FFF (16 KB)
- Application region: 0x08004000 - end
- UART: 115200 8N1, interrupt-driven RX (skeleton)
- Protocol: START, SIZE, DATA chunks (256 bytes), END, CHKSUM
- Integrity: QuickSort + adjacent XOR checksum

Quickstart (simulated build)

1. Build a simulator/native binary (requires arm-none-eabi-gcc or native gcc if adapted):

   make sim

2. For simulated runs the flash is backed by /tmp/uart_bootloader_sim_flash.bin and UART I/O is stdin/stdout.

Hardware notes
- The provided drivers include hardware skeletons with TODO comments where MCU-specific register code
  needs to be added for GPIO, USART and Flash programming. The simulation mode (USE_SIM) provides a
  working environment to test the protocol and the QuickSort checksum logic on the host.

Next steps
- Fill in hardware-specific register addresses and flash programming sequences for the STM32L496.
- Add interrupt handlers and circular RX buffer for UART.
- Add unit tests and optimize QuickSort for the 200 ms requirement if needed.
