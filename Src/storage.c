#include "storage.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

/* 1 KByte 24C08-style EEPROM
 * - 4 x 256-byte blocks selected in the device address
 * - 8-bit memory address within each block
 * - 16-byte page writes
 */
#define EEPROM_BASE_ADDR        (0x50U << 1)
#define EEPROM_BLOCK_SIZE       256U
#define EEPROM_PAGE_SIZE        16U
#define EEPROM_TIMEOUT_MS       100U
#define EEPROM_WRITE_DELAY_MS   10U

static uint16_t storage_device_addr_for_offset(uint16_t offset)
{
    return (uint16_t)(EEPROM_BASE_ADDR + (((offset / EEPROM_BLOCK_SIZE) & 0x07U) << 1));
}

static uint16_t storage_mem_addr_for_offset(uint16_t offset)
{
    return (uint16_t)(offset % EEPROM_BLOCK_SIZE);
}

int storage_read_bytes(uint16_t base_addr, uint8_t *dst, uint32_t len)
{
    uint32_t offset = 0U;

    if ((dst == NULL) || ((uint32_t)base_addr + len > STORAGE_EEPROM_TOTAL_SIZE_BYTES))
    {
        return -1;
    }

    while (offset < len)
    {
        uint16_t absolute_addr = (uint16_t)(base_addr + offset);
        uint32_t block_remaining = EEPROM_BLOCK_SIZE - storage_mem_addr_for_offset(absolute_addr);
        uint16_t chunk = (uint16_t)(((len - offset) < block_remaining) ? (len - offset) : block_remaining);

        if (HAL_I2C_Mem_Read(&hi2c1,
                             storage_device_addr_for_offset(absolute_addr),
                             storage_mem_addr_for_offset(absolute_addr),
                             I2C_MEMADD_SIZE_8BIT,
                             &dst[offset],
                             chunk,
                             EEPROM_TIMEOUT_MS) != HAL_OK)
        {
            return -1;
        }

        offset += chunk;
    }

    return 0;
}

int storage_write_bytes(uint16_t base_addr, const uint8_t *src, uint32_t len)
{
    uint32_t offset = 0U;

    if ((src == NULL) || ((uint32_t)base_addr + len > STORAGE_EEPROM_TOTAL_SIZE_BYTES))
    {
        return -1;
    }

    while (offset < len)
    {
        uint16_t absolute_addr = (uint16_t)(base_addr + offset);
        uint16_t page_offset = storage_mem_addr_for_offset(absolute_addr) % EEPROM_PAGE_SIZE;
        uint16_t page_remaining = (uint16_t)(EEPROM_PAGE_SIZE - page_offset);
        uint16_t block_remaining = (uint16_t)(EEPROM_BLOCK_SIZE - storage_mem_addr_for_offset(absolute_addr));
        uint16_t chunk = (uint16_t)(len - offset);

        if (chunk > page_remaining)
        {
            chunk = page_remaining;
        }
        if (chunk > block_remaining)
        {
            chunk = block_remaining;
        }

        if (HAL_I2C_Mem_Write(&hi2c1,
                              storage_device_addr_for_offset(absolute_addr),
                              storage_mem_addr_for_offset(absolute_addr),
                              I2C_MEMADD_SIZE_8BIT,
                              (uint8_t *)&src[offset],
                              chunk,
                              EEPROM_TIMEOUT_MS) != HAL_OK)
        {
            return -1;
        }

        HAL_Delay(EEPROM_WRITE_DELAY_MS);
        offset += chunk;
    }

    return 0;
}

/* Backward-compatible wrappers */
int storage_read_settings(uint8_t *dst, uint32_t len)
{
    return storage_read_bytes(0x0000U, dst, len);
}

int storage_write_settings(const uint8_t *src, uint32_t len)
{
    return storage_write_bytes(0x0000U, src, len);
}
