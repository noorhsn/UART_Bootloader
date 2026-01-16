#include "flash.h"
#include <stdint.h>
#include <string.h>
#include "config.h"

#ifdef USE_SIM
#include <stdio.h>
#include <stdlib.h>
#define SIM_FLASH_FILE "/tmp/uart_bootloader_sim_flash.bin"

static FILE *ensure_sim_flash(void) {
    FILE *f = fopen(SIM_FLASH_FILE, "r+b");
    if (!f) {
        f = fopen(SIM_FLASH_FILE, "w+b");
        if (!f) return NULL;
        // allocate 256KB
        fseek(f, 256*1024-1, SEEK_SET);
        fputc('\0', f);
        fflush(f);
    }
    return f;
}

int flash_erase_app_region(void) {
    FILE *f = ensure_sim_flash();
    if (!f) return -1;
    // zero the application region
    fseek(f, APP_START - BOOTLOADER_START, SEEK_SET);
    size_t zero_size = 256*1024 - (APP_START - BOOTLOADER_START);
    uint8_t *z = calloc(1, zero_size);
    if (!z) { fclose(f); return -1; }
    fwrite(z, 1, zero_size, f);
    free(z);
    fflush(f);
    fclose(f);
    return 0;
}

int flash_program(uint32_t address, const uint8_t *data, uint32_t len) {
    FILE *f = ensure_sim_flash();
    if (!f) return -1;
    uint32_t off = address - BOOTLOADER_START;
    fseek(f, off, SEEK_SET);
    fwrite(data, 1, len, f);
    fflush(f);
    fclose(f);
    return 0;
}

int flash_verify(uint32_t address, const uint8_t *data, uint32_t len) {
    FILE *f = ensure_sim_flash();
    if (!f) return -1;
    uint32_t off = address - BOOTLOADER_START;
    fseek(f, off, SEEK_SET);
    uint8_t *buf = malloc(len);
    if (!buf) { fclose(f); return -1; }
    size_t r = fread(buf, 1, len, f);
    int ok = (r == len && memcmp(buf, data, len) == 0);
    free(buf);
    fclose(f);
    return ok ? 0 : -2;
}

#else

/* Hardware flash programming skeleton for STM32L4 series.
   Implement unlock, erase, program and verify sequences according to
   the reference manual for your target MCU. These operations are MCU
   specific and require care (ECC, alignment, page size, etc.).
*/

int flash_erase_app_region(void) {
    // TODO: Erase pages in application region
    return 0;
}

int flash_program(uint32_t address, const uint8_t *data, uint32_t len) {
    // TODO: Program flash with proper alignment and half-word/word writes
    (void)address; (void)data; (void)len;
    return 0;
}

int flash_verify(uint32_t address, const uint8_t *data, uint32_t len) {
    // TODO: Read back flash and compare
    (void)address; (void)data; (void)len;
    return 0;
}

#endif
