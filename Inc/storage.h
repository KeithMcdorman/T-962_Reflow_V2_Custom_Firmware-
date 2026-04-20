#ifndef STORAGE_H
#define STORAGE_H

#include "main.h"
#include <stdint.h>

#define STORAGE_EEPROM_TOTAL_SIZE_BYTES 1024U

int storage_read_bytes(uint16_t base_addr, uint8_t *dst, uint32_t len);
int storage_write_bytes(uint16_t base_addr, const uint8_t *src, uint32_t len);

int storage_read_settings(uint8_t *dst, uint32_t len);
int storage_write_settings(const uint8_t *src, uint32_t len);

#endif
