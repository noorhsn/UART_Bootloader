/* Project configuration and memory map for UART Bootloader */
#ifndef CONFIG_H
#define CONFIG_H

// Bootloader layout
#define BOOTLOADER_START 0x08000000
#define BOOTLOADER_SIZE  0x4000      /* 16 KB */
#define APP_START         0x08004000

// Protocol parameters
#define CHUNK_SIZE 256

// UART
#define UART_BAUD 115200

#endif // CONFIG_H
