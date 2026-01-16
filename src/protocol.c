#include "protocol.h"
#include "uart.h"
#include "flash.h"
#include "quicksort.h"
#include "config.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Simple protocol constants (ASCII for readability) */
#define CMD_START "START"
#define RSP_READY "READY"
#define CMD_SIZE  "SIZE"
#define RSP_ACK   "ACK"
#define CMD_END   "END"
#define RSP_OK    "OK"
#define RSP_FAIL  "CHECKSUM_FAIL"

/* For simulation we will read from stdin; in hardware the uart driver
   provides byte streams and interrupt-driven RX. This protocol layer
   is intentionally small and synchronous for clarity. */

static uint8_t chunk_buf[CHUNK_SIZE];

int protocol_run(void) {
    uart_tx_blocking("BOOTLOADER_READY\n");
    // Wait for START
    char cmd[16] = {0};
    int n = uart_rx_nonblocking((uint8_t*)cmd, sizeof(cmd)-1);
    if (n <= 0) return PROTO_ERR;
    if (strncmp(cmd, CMD_START, strlen(CMD_START)) != 0) return PROTO_ERR;

    uart_tx((const uint8_t*)RSP_READY, strlen(RSP_READY));

    // Read SIZE command (format: SIZE:<bytes>\n)
    char sizebuf[32] = {0};
    n = uart_rx_nonblocking((uint8_t*)sizebuf, sizeof(sizebuf)-1);
    if (n <= 0) return PROTO_ERR;
    uint32_t fw_size = 0;
    if (sscanf(sizebuf, "SIZE:%u", &fw_size) != 1) return PROTO_ERR;

    // Erase application region
    if (flash_erase_app_region() != 0) return PROTO_ERR;

    // Acknowledge
    uart_tx((const uint8_t*)RSP_ACK, strlen(RSP_ACK));

    uint32_t written = 0;
    uint32_t addr = APP_START;
    while (written < fw_size) {
        int want = CHUNK_SIZE;
        if (fw_size - written < (uint32_t)CHUNK_SIZE) want = fw_size - written;
        int r = uart_rx_nonblocking(chunk_buf, want);
        if (r <= 0) return PROTO_ERR;
        // write to flash
        if (flash_program(addr, chunk_buf, r) != 0) return PROTO_ERR;
        if (flash_verify(addr, chunk_buf, r) != 0) return PROTO_ERR;
        addr += r;
        written += r;
        // send chunk ACK
        uart_tx((const uint8_t*)RSP_ACK, strlen(RSP_ACK));
    }

    // Receive END
    char endbuf[8] = {0};
    n = uart_rx_nonblocking((uint8_t*)endbuf, sizeof(endbuf)-1);
    if (n <= 0 || strncmp(endbuf, CMD_END, strlen(CMD_END)) != 0) return PROTO_ERR;

    // Host should send checksum (single byte as decimal: CHKSUM:<val> )
    char chkbuf[32] = {0};
    n = uart_rx_nonblocking((uint8_t*)chkbuf, sizeof(chkbuf)-1);
    if (n <= 0) return PROTO_ERR;
    unsigned host_chk = 0;
    if (sscanf(chkbuf, "CHKSUM:%u", &host_chk) != 1) return PROTO_ERR;

    // Read firmware from flash into buffer (use heap but restrained)
    uint8_t *fw = malloc(fw_size);
    if (!fw) return PROTO_ERR;
    // For SIM we can read from simulated flash file via flash_verify trick
    // Implement a simple read by writing into fw via flash_verify's internal read in sim
    // For now, we'll re-open sim flash file directly (only in sim)
#ifdef USE_SIM
    FILE *f = fopen("/tmp/uart_bootloader_sim_flash.bin", "rb");
    if (!f) { free(fw); return PROTO_ERR; }
    fseek(f, APP_START - BOOTLOADER_START, SEEK_SET);
    size_t rr = fread(fw, 1, fw_size, f);
    fclose(f);
    if (rr != fw_size) { free(fw); return PROTO_ERR; }
#else
    // In hardware mode, direct copy from flash memory region
    memcpy(fw, (const void*)APP_START, fw_size);
#endif

    // Compute QuickSort + checksum
    uint8_t chk = compute_sorted_xor_checksum(fw, fw_size);
    free(fw);

    if (chk != (uint8_t)host_chk) {
        uart_tx((const uint8_t*)RSP_FAIL, strlen(RSP_FAIL));
        return PROTO_ERR;
    }

    uart_tx((const uint8_t*)RSP_OK, strlen(RSP_OK));
    return PROTO_OK;
}
