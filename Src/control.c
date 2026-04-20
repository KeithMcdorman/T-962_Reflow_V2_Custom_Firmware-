#include "control.h"
#include "faults.h"
#include "settings.h"

#define CONTROL_DEFAULT_TARGET_C_X10       750
#define CONTROL_DEFAULT_DEADBAND_C_X10      50
#define CONTROL_DEFAULT_COOL_MARGIN_C_X10  100
#define CONTROL_HEAT_RAMP_FULL_C_X10      200
#define CONTROL_HEAT_RAMP_MIN_DUTY_PCT     20

static control_status_t g_control;
static uint8_t g_target_override_enabled;
static int16_t g_target_override_c_x10;
static uint8_t g_target_reached_latch;
static int16_t g_last_target_c_x10;

void control_init(void)
{
    const settings_data_t *settings = settings_get();

    g_control.enabled = 0U;
    g_control.target_c_x10 = settings->hold_target_c_x10;
    g_control.deadband_c_x10 = settings->control_deadband_c_x10;
    g_control.cool_margin_c_x10 = settings->control_cool_margin_c_x10;
    g_control.control_temp_c_x10 = 0;
    g_control.heat_request = 0U;
    g_control.fan_request = 0U;
    g_control.heat_duty_percent = 0U;
    g_target_override_enabled = 0U;
    g_target_override_c_x10 = 0;
    g_target_reached_latch = 0U;
    g_last_target_c_x10 = g_control.target_c_x10;

    if (g_control.target_c_x10 <= 0)
    {
        g_control.target_c_x10 = CONTROL_DEFAULT_TARGET_C_X10;
    }
    if (g_control.deadband_c_x10 <= 0)
    {
        g_control.deadband_c_x10 = CONTROL_DEFAULT_DEADBAND_C_X10;
    }
    if (g_control.cool_margin_c_x10 <= 0)
    {
        g_control.cool_margin_c_x10 = CONTROL_DEFAULT_COOL_MARGIN_C_X10;
    }
}

void control_set_enabled(uint8_t enabled)
{
    g_control.enabled = (enabled != 0U) ? 1U : 0U;
    if (g_control.enabled == 0U)
    {
        g_control.heat_request = 0U;
        g_control.fan_request = 0U;
        g_control.heat_duty_percent = 0U;
        g_target_reached_latch = 0U;
    }
}

void control_toggle_enabled(void)
{
    control_set_enabled((g_control.enabled == 0U) ? 1U : 0U);
}

void control_set_target_override(uint8_t enabled, int16_t target_c_x10)
{
    g_target_override_enabled = (enabled != 0U) ? 1U : 0U;
    g_target_override_c_x10 = target_c_x10;
}

void control_update(int16_t left_c_x10,
                    int16_t right_c_x10,
                    uint32_t fault_bits,
                    uint8_t fault_critical)
{
    const settings_data_t *settings = settings_get();
    uint8_t left_valid = ((fault_bits & FAULT_LEFT_SENSOR) == 0U) ? 1U : 0U;
    uint8_t right_valid = ((fault_bits & FAULT_RIGHT_SENSOR) == 0U) ? 1U : 0U;
    int16_t control_temp = 0;

    g_control.target_c_x10 = settings->hold_target_c_x10;
    g_control.deadband_c_x10 = settings->control_deadband_c_x10;
    g_control.cool_margin_c_x10 = settings->control_cool_margin_c_x10;

    if (g_control.target_c_x10 <= 0)
    {
        g_control.target_c_x10 = CONTROL_DEFAULT_TARGET_C_X10;
    }
    if (g_control.deadband_c_x10 <= 0)
    {
        g_control.deadband_c_x10 = CONTROL_DEFAULT_DEADBAND_C_X10;
    }
    if (g_control.cool_margin_c_x10 <= 0)
    {
        g_control.cool_margin_c_x10 = CONTROL_DEFAULT_COOL_MARGIN_C_X10;
    }
    if (g_target_override_enabled != 0U)
    {
        g_control.target_c_x10 = g_target_override_c_x10;
    }

    if (g_control.target_c_x10 != g_last_target_c_x10)
    {
        g_target_reached_latch = 0U;
        g_last_target_c_x10 = g_control.target_c_x10;
    }

    g_control.heat_request = 0U;
    g_control.fan_request = 0U;
    g_control.heat_duty_percent = 0U;

    if (left_valid != 0U && right_valid != 0U)
    {
        control_temp = (left_c_x10 > right_c_x10) ? left_c_x10 : right_c_x10;
    }
    else if (left_valid != 0U)
    {
        control_temp = left_c_x10;
    }
    else if (right_valid != 0U)
    {
        control_temp = right_c_x10;
    }
    else
    {
        g_control.control_temp_c_x10 = 0;
        return;
    }

    g_control.control_temp_c_x10 = control_temp;

    if ((g_control.enabled == 0U) || (fault_critical != 0U))
    {
        return;
    }

    if (control_temp >= g_control.target_c_x10)
    {
        g_target_reached_latch = 1U;
    }

    if (control_temp < g_control.target_c_x10)
    {
        uint8_t duty_pct = 100U;

        g_control.heat_request = 1U;
        g_control.fan_request = 0U;

        if (g_target_reached_latch != 0U)
        {
            int16_t error_c_x10 = (int16_t)(g_control.target_c_x10 - control_temp);

            if (error_c_x10 <= g_control.deadband_c_x10)
            {
                duty_pct = 0U;
            }
            else if (error_c_x10 < CONTROL_HEAT_RAMP_FULL_C_X10)
            {
                int32_t ramp_span = (int32_t)CONTROL_HEAT_RAMP_FULL_C_X10 - (int32_t)g_control.deadband_c_x10;

                if (ramp_span > 0)
                {
                    int32_t scaled = ((int32_t)(error_c_x10 - g_control.deadband_c_x10)
                                      * (100 - CONTROL_HEAT_RAMP_MIN_DUTY_PCT)) / ramp_span;
                    duty_pct = (uint8_t)(CONTROL_HEAT_RAMP_MIN_DUTY_PCT + scaled);
                }
            }
        }

        if (duty_pct > 100U)
        {
            duty_pct = 100U;
        }

        g_control.heat_duty_percent = duty_pct;
    }
    else if (control_temp >= (g_control.target_c_x10 + g_control.cool_margin_c_x10))
    {
        g_control.heat_request = 0U;
        g_control.fan_request = 1U;
        g_control.heat_duty_percent = 0U;
    }
    else
    {
        g_control.heat_request = 0U;
        g_control.fan_request = 0U;
        g_control.heat_duty_percent = 0U;
    }
}

const control_status_t *control_get_status(void)
{
    return &g_control;
}
