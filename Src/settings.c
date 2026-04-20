#include "settings.h"
#include "storage.h"
#include <string.h>

#define SETTINGS_DEFAULT_HOLD_TARGET_C_X10       750
#define SETTINGS_DEFAULT_DEADBAND_C_X10           50
#define SETTINGS_DEFAULT_COOL_MARGIN_C_X10       100

static settings_data_t g_settings;
static uint32_t g_settings_seq = 0U;
static uint16_t g_active_slot_addr = SETTINGS_SLOT_A_ADDR;

typedef struct
{
    uint16_t version;
    uint16_t crc;
    uint32_t sequence;
    settings_data_t data;
} settings_block_t;

static uint16_t settings_crc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8U; j++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static int settings_block_is_valid(const settings_block_t *block)
{
    uint16_t crc = settings_crc((const uint8_t *)&block->data, sizeof(block->data));

    if (block->version != SETTINGS_VERSION)
    {
        return 0;
    }

    if (crc != block->crc)
    {
        return 0;
    }

    return 1;
}

void settings_set_defaults(void)
{
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.left_gain_x1000 = 1000;
    g_settings.left_offset_c_x10 = 0;
    g_settings.right_gain_x1000 = 1000;
    g_settings.right_offset_c_x10 = 0;
    g_settings.hold_target_c_x10 = SETTINGS_DEFAULT_HOLD_TARGET_C_X10;
    g_settings.control_deadband_c_x10 = SETTINGS_DEFAULT_DEADBAND_C_X10;
    g_settings.control_cool_margin_c_x10 = SETTINGS_DEFAULT_COOL_MARGIN_C_X10;
    g_settings.display_units_f = 0U;
}

const settings_data_t *settings_get(void)
{
    return &g_settings;
}

void settings_set_display_units_f(uint8_t display_units_f)
{
    g_settings.display_units_f = (display_units_f != 0U) ? 1U : 0U;
}

void settings_set_left_calibration(int32_t gain_x1000, int16_t offset_c_x10)
{
    g_settings.left_gain_x1000 = gain_x1000;
    g_settings.left_offset_c_x10 = offset_c_x10;
}

void settings_set_right_calibration(int32_t gain_x1000, int16_t offset_c_x10)
{
    g_settings.right_gain_x1000 = gain_x1000;
    g_settings.right_offset_c_x10 = offset_c_x10;
}

void settings_set_hold_target_c_x10(int16_t target_c_x10)
{
    g_settings.hold_target_c_x10 = target_c_x10;
}

void settings_set_control_deadband_c_x10(int16_t deadband_c_x10)
{
    g_settings.control_deadband_c_x10 = deadband_c_x10;
}

void settings_set_control_cool_margin_c_x10(int16_t cool_margin_c_x10)
{
    g_settings.control_cool_margin_c_x10 = cool_margin_c_x10;
}

void settings_init(void)
{
    settings_block_t block_a;
    settings_block_t block_b;
    int valid_a;
    int valid_b;

    memset(&block_a, 0, sizeof(block_a));
    memset(&block_b, 0, sizeof(block_b));

    valid_a = (storage_read_bytes(SETTINGS_SLOT_A_ADDR, (uint8_t *)&block_a, sizeof(block_a)) == 0)
              && settings_block_is_valid(&block_a);

    valid_b = (storage_read_bytes(SETTINGS_SLOT_B_ADDR, (uint8_t *)&block_b, sizeof(block_b)) == 0)
              && settings_block_is_valid(&block_b);

    if (valid_a && valid_b)
    {
        if (block_b.sequence >= block_a.sequence)
        {
            memcpy(&g_settings, &block_b.data, sizeof(g_settings));
            g_settings_seq = block_b.sequence;
            g_active_slot_addr = SETTINGS_SLOT_B_ADDR;
        }
        else
        {
            memcpy(&g_settings, &block_a.data, sizeof(g_settings));
            g_settings_seq = block_a.sequence;
            g_active_slot_addr = SETTINGS_SLOT_A_ADDR;
        }
        return;
    }

    if (valid_a)
    {
        memcpy(&g_settings, &block_a.data, sizeof(g_settings));
        g_settings_seq = block_a.sequence;
        g_active_slot_addr = SETTINGS_SLOT_A_ADDR;
        return;
    }

    if (valid_b)
    {
        memcpy(&g_settings, &block_b.data, sizeof(g_settings));
        g_settings_seq = block_b.sequence;
        g_active_slot_addr = SETTINGS_SLOT_B_ADDR;
        return;
    }

    settings_set_defaults();
    g_settings_seq = 0U;
    g_active_slot_addr = SETTINGS_SLOT_A_ADDR;
}

void settings_save(void)
{
    settings_block_t block;
    uint16_t target_slot_addr;

    memset(&block, 0, sizeof(block));
    block.version = SETTINGS_VERSION;
    block.sequence = g_settings_seq + 1U;
    memcpy(&block.data, &g_settings, sizeof(g_settings));
    block.crc = settings_crc((const uint8_t *)&block.data, sizeof(block.data));

    target_slot_addr = (g_active_slot_addr == SETTINGS_SLOT_A_ADDR) ? SETTINGS_SLOT_B_ADDR : SETTINGS_SLOT_A_ADDR;

    if (storage_write_bytes(target_slot_addr, (const uint8_t *)&block, sizeof(block)) == 0)
    {
        g_settings_seq = block.sequence;
        g_active_slot_addr = target_slot_addr;
    }
}
