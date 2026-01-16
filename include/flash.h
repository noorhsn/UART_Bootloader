#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

int flash_erase_app_region(void);
int flash_program(uint32_t address, const uint8_t *data, uint32_t len);
int flash_verify(uint32_t address, const uint8_t *data, uint32_t len);

#endif // FLASH_H
