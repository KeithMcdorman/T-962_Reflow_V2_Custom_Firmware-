#include "profiles.h"
#include "storage.h"
#include "settings.h"
#include <string.h>

#define PROFILE_VERSION            0x0001U
#define PROFILE_SLOT_A_ADDR        0x0100U
#define PROFILE_SLOT_B_ADDR        0x0180U
#define PROFILE_DEFAULT_STEP_COUNT 4U
#define PROFILE_MIN_STEP_COUNT     1U
#define PROFILE_MAX_STEP_COUNT_UI  PROFILE_MAX_STEPS
#define PROFILE_TARGET_MIN_C_X10   300
#define PROFILE_TARGET_MAX_C_X10   2600
#define PROFILE_DURATION_MIN_S     5
#define PROFILE_DURATION_MAX_S     600

typedef struct
{
    uint16_t version;
    uint16_t crc;
    uint32_t sequence;
    profile_t profile;
} profile_block_t;

static const profile_t g_default_profile =
{
    .name = "SAC305",
    .steps =
    {
        { 1200, 90000U },
        { 1500, 90000U },
        { 2200, 45000U },
        { 1800, 60000U },
        {    0,     0U }
    },
    .step_count = PROFILE_DEFAULT_STEP_COUNT
};

static const profile_t g_sn63pb37_profile =
{
    .name = "SN63PB37",
    .steps =
    {
        { 1100, 90000U },
        { 1400, 90000U },
        { 2050, 35000U },
        { 1700, 50000U },
        {    0,     0U }
    },
    .step_count = PROFILE_DEFAULT_STEP_COUNT
};

static const profile_t g_sn60pb40_profile =
{
    .name = "SN60PB40",
    .steps =
    {
        { 1100, 90000U },
        { 1450, 90000U },
        { 2100, 40000U },
        { 1700, 50000U },
        {    0,     0U }
    },
    .step_count = PROFILE_DEFAULT_STEP_COUNT
};

static const profile_t g_lowtemp_profile =
{
    .name = "LOWTEMP",
    .steps =
    {
        { 1000, 90000U },
        { 1300, 90000U },
        { 1800, 50000U },
        { 1500, 50000U },
        {    0,     0U }
    },
    .step_count = PROFILE_DEFAULT_STEP_COUNT
};

static profile_t g_profile;
static profile_t g_saved_profile;
static profile_status_t g_status;
static uint32_t g_profile_start_ms;
static uint32_t g_step_start_ms;
static uint32_t g_pause_start_ms;
static uint8_t g_step_timer_started;
static uint8_t g_edit_step;
static uint32_t g_profile_seq;
static uint16_t g_profile_active_slot_addr;

static uint16_t profiles_crc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t j;

    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++)
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

static int profiles_block_is_valid(const profile_block_t *block)
{
    if (block->version != PROFILE_VERSION)
    {
        return 0;
    }

    if ((block->profile.step_count < PROFILE_MIN_STEP_COUNT) ||
        (block->profile.step_count > PROFILE_MAX_STEP_COUNT_UI))
    {
        return 0;
    }

    if (profiles_crc((const uint8_t *)&block->profile, sizeof(block->profile)) != block->crc)
    {
        return 0;
    }

    return 1;
}

static void profiles_copy_defaults(profile_t *dst)
{
    memcpy(dst, &g_default_profile, sizeof(*dst));
}

static const profile_t *profiles_get_builtin_profile(profile_builtin_id_t builtin_id)
{
    switch (builtin_id)
    {
        case PROFILE_BUILTIN_SN63PB37:
            return &g_sn63pb37_profile;

        case PROFILE_BUILTIN_SN60PB40:
            return &g_sn60pb40_profile;

        case PROFILE_BUILTIN_LOWTEMP:
            return &g_lowtemp_profile;

        case PROFILE_BUILTIN_SAC305:
        default:
            return &g_default_profile;
    }
}

const char *profiles_get_builtin_name(profile_builtin_id_t builtin_id)
{
    return profiles_get_builtin_profile(builtin_id)->name;
}

static void profiles_select_working_profile(const profile_t *src)
{
    memcpy(&g_profile, src, sizeof(g_profile));

    if ((g_profile.step_count < PROFILE_MIN_STEP_COUNT) ||
        (g_profile.step_count > PROFILE_MAX_STEP_COUNT_UI))
    {
        profiles_copy_defaults(&g_profile);
    }

    if (g_edit_step >= g_profile.step_count)
    {
        g_edit_step = (uint8_t)(g_profile.step_count - 1U);
    }

    g_status.step_count = g_profile.step_count;
}

static void profiles_load_step(uint8_t step_index, uint32_t now_ms)
{
    g_status.current_step = step_index;
    g_status.current_target_c_x10 = g_profile.steps[step_index].target_c_x10;
    g_step_start_ms = now_ms;
    g_step_timer_started = 0U;
    g_status.step_remaining_ms = g_profile.steps[step_index].duration_ms;
}

static int16_t clamp_i16(int16_t value, int16_t min_v, int16_t max_v)
{
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

void profiles_init(void)
{
    profile_block_t block_a;
    profile_block_t block_b;
    int valid_a;
    int valid_b;

    memset(&g_status, 0, sizeof(g_status));
    memset(&block_a, 0, sizeof(block_a));
    memset(&block_b, 0, sizeof(block_b));

    valid_a = (storage_read_bytes(PROFILE_SLOT_A_ADDR, (uint8_t *)&block_a, sizeof(block_a)) == 0)
              && profiles_block_is_valid(&block_a);
    valid_b = (storage_read_bytes(PROFILE_SLOT_B_ADDR, (uint8_t *)&block_b, sizeof(block_b)) == 0)
              && profiles_block_is_valid(&block_b);

    if (valid_a && valid_b)
    {
        if (block_b.sequence >= block_a.sequence)
        {
            memcpy(&g_saved_profile, &block_b.profile, sizeof(g_saved_profile));
            g_profile_seq = block_b.sequence;
            g_profile_active_slot_addr = PROFILE_SLOT_B_ADDR;
        }
        else
        {
            memcpy(&g_saved_profile, &block_a.profile, sizeof(g_saved_profile));
            g_profile_seq = block_a.sequence;
            g_profile_active_slot_addr = PROFILE_SLOT_A_ADDR;
        }
    }
    else if (valid_a)
    {
        memcpy(&g_saved_profile, &block_a.profile, sizeof(g_saved_profile));
        g_profile_seq = block_a.sequence;
        g_profile_active_slot_addr = PROFILE_SLOT_A_ADDR;
    }
    else if (valid_b)
    {
        memcpy(&g_saved_profile, &block_b.profile, sizeof(g_saved_profile));
        g_profile_seq = block_b.sequence;
        g_profile_active_slot_addr = PROFILE_SLOT_B_ADDR;
    }
    else
    {
        profiles_copy_defaults(&g_saved_profile);
        memcpy(&g_profile, &g_saved_profile, sizeof(g_profile));
        g_profile_seq = 0U;
        g_profile_active_slot_addr = PROFILE_SLOT_A_ADDR;
        profiles_save();
    }

    if ((g_saved_profile.step_count < PROFILE_MIN_STEP_COUNT) || (g_saved_profile.step_count > PROFILE_MAX_STEP_COUNT_UI))
    {
        profiles_copy_defaults(&g_saved_profile);
    }

    g_edit_step = 0U;
    profiles_select_working_profile(&g_saved_profile);
}

void profiles_start(uint32_t now_ms)
{
    memset(&g_status, 0, sizeof(g_status));
    g_status.active = 1U;
    g_status.paused = 0U;
    g_pause_start_ms = 0U;
    g_status.step_count = g_profile.step_count;
    g_profile_start_ms = now_ms;
    g_status.step_elapsed_ms = 0U;
    profiles_load_step(0U, now_ms);
}

void profiles_abort(void)
{
    g_status.active = 0U;
    g_status.paused = 0U;
    g_status.current_target_c_x10 = 0;
    g_status.step_elapsed_ms = 0U;
    g_status.step_remaining_ms = 0U;
    g_step_timer_started = 0U;
}


void profiles_pause(uint32_t now_ms)
{
    if ((g_status.active == 0U) || (g_status.paused != 0U))
    {
        return;
    }

    g_status.paused = 1U;
    g_pause_start_ms = now_ms;
}

void profiles_resume(uint32_t now_ms)
{
    uint32_t paused_ms;

    if ((g_status.active == 0U) || (g_status.paused == 0U))
    {
        return;
    }

    paused_ms = now_ms - g_pause_start_ms;
    g_profile_start_ms += paused_ms;
    if (g_step_timer_started != 0U)
    {
        g_step_start_ms += paused_ms;
    }
    g_status.paused = 0U;
    g_pause_start_ms = 0U;
}

void profiles_toggle_pause(uint32_t now_ms)
{
    if (g_status.paused != 0U)
    {
        profiles_resume(now_ms);
    }
    else
    {
        profiles_pause(now_ms);
    }
}

void profiles_update(uint32_t now_ms, uint8_t fault_critical, int16_t current_temp_c_x10)
{
    uint32_t duration_ms;
    uint32_t elapsed_ms;

    if ((fault_critical != 0U) && (g_status.active != 0U))
    {
        profiles_abort();
        return;
    }

    if (g_status.active == 0U)
    {
        return;
    }

    if (g_status.paused != 0U)
    {
        return;
    }

    duration_ms = g_profile.steps[g_status.current_step].duration_ms;
    g_status.step_elapsed_ms = now_ms - g_profile_start_ms;

    if (g_step_timer_started == 0U)
    {
        int16_t start_threshold_c_x10;
        int16_t deadband_c_x10 = settings_get()->control_deadband_c_x10;

        if (deadband_c_x10 <= 0)
        {
            deadband_c_x10 = 10;
        }

        start_threshold_c_x10 = (int16_t)(g_status.current_target_c_x10 - deadband_c_x10);
        g_status.step_remaining_ms = duration_ms;

        if (current_temp_c_x10 < start_threshold_c_x10)
        {
            return;
        }

        g_step_timer_started = 1U;
        g_step_start_ms = now_ms;
    }

    elapsed_ms = now_ms - g_step_start_ms;
    g_status.step_remaining_ms = (elapsed_ms < duration_ms) ? (duration_ms - elapsed_ms) : 0U;

    if (elapsed_ms < duration_ms)
    {
        return;
    }

    if ((uint8_t)(g_status.current_step + 1U) >= g_profile.step_count)
    {
        g_status.active = 0U;
        g_status.completed = 1U;
        g_status.paused = 0U;
        g_status.current_target_c_x10 = 0;
        g_status.step_elapsed_ms = now_ms - g_profile_start_ms;
        g_status.step_remaining_ms = 0U;
        g_step_timer_started = 0U;
        return;
    }

    profiles_load_step((uint8_t)(g_status.current_step + 1U), now_ms);
}

const profile_status_t *profiles_get_status(void)
{
    return &g_status;
}

const profile_t *profiles_get_profile(void)
{
    return &g_profile;
}

void profiles_set_edit_step(uint8_t step_index)
{
    if (step_index >= g_profile.step_count)
    {
        if (g_profile.step_count == 0U)
        {
            g_edit_step = 0U;
        }
        else
        {
            g_edit_step = (uint8_t)(g_profile.step_count - 1U);
        }
    }
    else
    {
        g_edit_step = step_index;
    }
}

uint8_t profiles_get_edit_step(void)
{
    return g_edit_step;
}

void profiles_adjust_edit_target_c_x10(int16_t delta_c_x10)
{
    int16_t value = (int16_t)(g_profile.steps[g_edit_step].target_c_x10 + delta_c_x10);
    g_profile.steps[g_edit_step].target_c_x10 = clamp_i16(value, PROFILE_TARGET_MIN_C_X10, PROFILE_TARGET_MAX_C_X10);
}

void profiles_adjust_edit_duration_s(int16_t delta_s)
{
    int32_t value_s = (int32_t)(g_profile.steps[g_edit_step].duration_ms / 1000U) + delta_s;

    if (value_s < PROFILE_DURATION_MIN_S) value_s = PROFILE_DURATION_MIN_S;
    if (value_s > PROFILE_DURATION_MAX_S) value_s = PROFILE_DURATION_MAX_S;

    g_profile.steps[g_edit_step].duration_ms = (uint32_t)value_s * 1000U;
}

void profiles_adjust_step_count(int8_t delta)
{
    int16_t count = (int16_t)g_profile.step_count + delta;
    uint8_t i;

    if (count < PROFILE_MIN_STEP_COUNT) count = PROFILE_MIN_STEP_COUNT;
    if (count > PROFILE_MAX_STEP_COUNT_UI) count = PROFILE_MAX_STEP_COUNT_UI;
    g_profile.step_count = (uint8_t)count;

    for (i = g_profile.step_count; i < PROFILE_MAX_STEPS; i++)
    {
        g_profile.steps[i].target_c_x10 = 0;
        g_profile.steps[i].duration_ms = 0U;
    }

    if (g_edit_step >= g_profile.step_count)
    {
        g_edit_step = (uint8_t)(g_profile.step_count - 1U);
    }
}

void profiles_save(void)
{
    profile_block_t block;
    uint16_t target_slot_addr;

    memset(&block, 0, sizeof(block));
    block.version = PROFILE_VERSION;
    block.sequence = g_profile_seq + 1U;
    memcpy(&block.profile, &g_profile, sizeof(g_profile));
    block.crc = profiles_crc((const uint8_t *)&block.profile, sizeof(block.profile));

    target_slot_addr = (g_profile_active_slot_addr == PROFILE_SLOT_A_ADDR) ? PROFILE_SLOT_B_ADDR : PROFILE_SLOT_A_ADDR;

    if (storage_write_bytes(target_slot_addr, (const uint8_t *)&block, sizeof(block)) == 0)
    {
        g_profile_seq = block.sequence;
        g_profile_active_slot_addr = target_slot_addr;
        memcpy(&g_saved_profile, &g_profile, sizeof(g_saved_profile));
    }
}

void profiles_restore_defaults(void)
{
    profiles_copy_defaults(&g_profile);
    g_edit_step = 0U;
    profiles_save();
}

void profiles_select_default(void)
{
    profiles_select_builtin(PROFILE_BUILTIN_SAC305);
}

void profiles_select_builtin(profile_builtin_id_t builtin_id)
{
    if (g_status.active != 0U)
    {
        return;
    }

    memcpy(&g_profile, profiles_get_builtin_profile(builtin_id), sizeof(g_profile));
    g_edit_step = 0U;
    g_status.completed = 0U;
    g_status.current_step = 0U;
    g_status.current_target_c_x10 = 0;
    g_status.step_elapsed_ms = 0U;
    g_status.step_remaining_ms = 0U;
    g_status.step_count = g_profile.step_count;
}

void profiles_select_saved(void)
{
    if (g_status.active != 0U)
    {
        return;
    }

    profiles_select_working_profile(&g_saved_profile);
    g_status.completed = 0U;
    g_status.current_step = 0U;
    g_status.current_target_c_x10 = 0;
    g_status.step_elapsed_ms = 0U;
    g_status.step_remaining_ms = 0U;
}
