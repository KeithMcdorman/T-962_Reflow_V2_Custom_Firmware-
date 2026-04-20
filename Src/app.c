#include "app.h"
#include "board.h"
#include "control.h"
#include "faults.h"
#include "profiles.h"
#include "sensors.h"
#include "settings.h"
#include "temps.h"
#include "ui.h"
#include <string.h>

#define APP_TEMP_UPDATE_MS 250U
#define APP_HEAT_PWM_WINDOW_MS 2000U
#define APP_TARGET_MIN_C_X10        0
#define APP_TARGET_MAX_C_X10     3000
#define APP_DEADBAND_MIN_C_X10      5
#define APP_DEADBAND_MAX_C_X10    300
#define APP_COOL_MARGIN_MIN_C_X10  10
#define APP_COOL_MARGIN_MAX_C_X10  500

#define APP_LEFT_GAIN_MIN_X1000    800
#define APP_LEFT_GAIN_MAX_X1000   1200
#define APP_RIGHT_GAIN_MIN_X1000   800
#define APP_RIGHT_GAIN_MAX_X1000  1200
#define APP_CAL_OFFSET_MIN_C_X10  -200
#define APP_CAL_OFFSET_MAX_C_X10   200

static app_status_t g_app;
static uint32_t g_last_temp_update_ms;
static uint8_t g_profile_was_active;

static int16_t app_clamp_i16(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static void app_apply_outputs(uint32_t now_ms)
{
    const control_status_t *control = control_get_status();
    uint8_t heat_on = 0U;
    uint8_t fan_on = 0U;

    if (g_app.fault_critical != 0U)
    {
        heat_on = 0U;
        fan_on = 1U;
    }
    else if (g_app.fan_forced_on != 0U)
    {
        heat_on = 0U;
        fan_on = 1U;
    }
    else
    {
        if (control->heat_request != 0U)
        {
            if (control->heat_duty_percent >= 100U)
            {
                heat_on = 1U;
            }
            else if (control->heat_duty_percent > 0U)
            {
                uint32_t window_pos_ms = now_ms % APP_HEAT_PWM_WINDOW_MS;
                uint32_t on_time_ms = ((uint32_t)control->heat_duty_percent * APP_HEAT_PWM_WINDOW_MS) / 100U;

                if (on_time_ms == 0U)
                {
                    on_time_ms = 1U;
                }
                if (window_pos_ms < on_time_ms)
                {
                    heat_on = 1U;
                }
            }
        }
        fan_on = control->fan_request;
    }

    if (heat_on != 0U)
    {
        fan_on = 0U;
    }

    board_heat_set(heat_on != 0U);
    board_fan_set(fan_on != 0U);
    g_app.heat_output_on = heat_on;
}

static void app_update_temperature_snapshot(void)
{
    sensor_adc_sample_t left_sample;
    sensor_adc_sample_t right_sample;
    sensor_adc_sample_t cj_sample;

    sensors_adc_sample_channel(ADC_CHANNEL_0, &left_sample);
    sensors_adc_sample_channel(ADC_CHANNEL_2, &right_sample);
    sensors_adc_sample_channel(ADC_CHANNEL_6, &cj_sample);

    g_app.temps.left_raw = left_sample.raw;
    g_app.temps.right_raw = right_sample.raw;
    g_app.temps.cj_raw = cj_sample.raw;
    g_app.temps.left_filtered_raw = left_sample.filtered_raw;
    g_app.temps.right_filtered_raw = right_sample.filtered_raw;
    g_app.temps.cj_filtered_raw = cj_sample.filtered_raw;

    g_app.temps.left_mv = left_sample.raw_mv;
    g_app.temps.right_mv = right_sample.raw_mv;
    g_app.temps.cj_mv = cj_sample.raw_mv;
    g_app.temps.left_filtered_mv = left_sample.filtered_mv;
    g_app.temps.right_filtered_mv = right_sample.filtered_mv;
    g_app.temps.cj_filtered_mv = cj_sample.filtered_mv;

    uint8_t left_valid;
    uint8_t right_valid;

    g_app.temps.cj_c_x10 = temps_cold_junction_mv_to_temp_c_x10(g_app.temps.cj_filtered_mv);
    g_app.temps.left_c_x10 = temps_left_probe_mv_to_temp_c_x10(g_app.temps.left_filtered_mv, g_app.temps.cj_c_x10);
    g_app.temps.right_c_x10 = temps_right_probe_mv_to_temp_c_x10(g_app.temps.right_filtered_mv, g_app.temps.cj_c_x10);

    faults_update(&g_app.temps, g_app.heat_output_on, HAL_GetTick());
    {
        const faults_status_t *faults = faults_get_status();
        g_app.fault_bits = faults->active_bits;
        g_app.fault_critical = faults->critical_active;
    }

    left_valid = ((g_app.fault_bits & FAULT_LEFT_SENSOR) == 0U) ? 1U : 0U;
    right_valid = ((g_app.fault_bits & FAULT_RIGHT_SENSOR) == 0U) ? 1U : 0U;

    if ((left_valid != 0U) && (right_valid != 0U))
    {
        g_app.temps.oven_avg_c_x10 = (int16_t)((g_app.temps.left_c_x10 + g_app.temps.right_c_x10) / 2);
        g_app.temps.oven_avg_valid = 1U;
    }
    else if (left_valid != 0U)
    {
        g_app.temps.oven_avg_c_x10 = g_app.temps.left_c_x10;
        g_app.temps.oven_avg_valid = 1U;
    }
    else if (right_valid != 0U)
    {
        g_app.temps.oven_avg_c_x10 = g_app.temps.right_c_x10;
        g_app.temps.oven_avg_valid = 1U;
    }
    else
    {
        g_app.temps.oven_avg_c_x10 = 0;
        g_app.temps.oven_avg_valid = 0U;
    }

    {
        int16_t profile_current_c_x10 = g_app.temps.oven_avg_valid != 0U
            ? g_app.temps.oven_avg_c_x10
            : (int16_t)((g_app.temps.left_c_x10 + g_app.temps.right_c_x10) / 2);
        profiles_update(HAL_GetTick(), g_app.fault_critical, profile_current_c_x10);
    }
    {
        const profile_status_t *profile = profiles_get_status();
        control_set_target_override(profile->active, profile->current_target_c_x10);
        if ((g_profile_was_active != 0U) && (profile->active == 0U))
        {
            control_set_enabled(0U);
        }
        g_profile_was_active = profile->active;
    }

    control_update(g_app.temps.left_c_x10,
                   g_app.temps.right_c_x10,
                   g_app.fault_bits,
                   g_app.fault_critical);
    {
        const control_status_t *control = control_get_status();
        g_app.control_enabled = control->enabled;
        g_app.control_heat_request = control->heat_request;
        g_app.control_fan_request = control->fan_request;
        if (g_app.display_units_f != 0U)
        {
            g_app.control_target_display_x10 = temps_c_to_f_x10(control->target_c_x10);
            g_app.control_temp_display_x10 = temps_c_to_f_x10(control->control_temp_c_x10);
            g_app.control_deadband_display_x10 = temps_c_to_f_x10(control->deadband_c_x10) - temps_c_to_f_x10(0);
            g_app.control_cool_margin_display_x10 = temps_c_to_f_x10(control->cool_margin_c_x10) - temps_c_to_f_x10(0);
        }
        else
        {
            g_app.control_target_display_x10 = control->target_c_x10;
            g_app.control_temp_display_x10 = control->control_temp_c_x10;
            g_app.control_deadband_display_x10 = control->deadband_c_x10;
            g_app.control_cool_margin_display_x10 = control->cool_margin_c_x10;
        }
    }

    {
        const profile_status_t *profile = profiles_get_status();
        int16_t profile_current_c_x10 = g_app.temps.oven_avg_valid != 0U
            ? g_app.temps.oven_avg_c_x10
            : (int16_t)((g_app.temps.left_c_x10 + g_app.temps.right_c_x10) / 2);
        g_app.profile_active = profile->active;
        g_app.profile_paused = profile->paused;
        g_app.profile_completed = profile->completed;
        g_app.profile_step_index = profile->current_step;
        g_app.profile_step_count = profile->step_count;
        g_app.profile_step_elapsed_s = profile->step_elapsed_ms / 1000U;
        g_app.profile_step_remaining_s = profile->step_remaining_ms / 1000U;
        if (g_app.display_units_f != 0U)
        {
            g_app.profile_target_display_x10 = temps_c_to_f_x10(profile->current_target_c_x10);
            g_app.profile_current_display_x10 = temps_c_to_f_x10(profile_current_c_x10);
        }
        else
        {
            g_app.profile_target_display_x10 = profile->current_target_c_x10;
            g_app.profile_current_display_x10 = profile_current_c_x10;
        }
    }

    if (g_app.display_units_f != 0U)
    {
        g_app.temps.left_display_x10 = temps_c_to_f_x10(g_app.temps.left_c_x10);
        g_app.temps.right_display_x10 = temps_c_to_f_x10(g_app.temps.right_c_x10);
        g_app.temps.cj_display_x10 = temps_c_to_f_x10(g_app.temps.cj_c_x10);
        g_app.temps.oven_avg_display_x10 = temps_c_to_f_x10(g_app.temps.oven_avg_c_x10);
    }
    else
    {
        g_app.temps.left_display_x10 = g_app.temps.left_c_x10;
        g_app.temps.right_display_x10 = g_app.temps.right_c_x10;
        g_app.temps.cj_display_x10 = g_app.temps.cj_c_x10;
        g_app.temps.oven_avg_display_x10 = g_app.temps.oven_avg_c_x10;
    }

    app_apply_outputs(HAL_GetTick());
}

void app_init(void)
{
    const settings_data_t *settings;

    memset(&g_app, 0, sizeof(g_app));
    board_all_outputs_off();
    settings_init();
    faults_init();
    profiles_init();
    control_init();
    settings = settings_get();
    g_app.display_units_f = settings->display_units_f;
    sensors_analog_pins_init();
    sensors_filter_reset();
    app_update_temperature_snapshot();
    ui_init();
}

void app_task(uint32_t now_ms)
{
    (void)now_ms;

    if ((now_ms - g_last_temp_update_ms) >= APP_TEMP_UPDATE_MS)
    {
        g_last_temp_update_ms = now_ms;
        app_update_temperature_snapshot();
    }

    app_apply_outputs(now_ms);
    ui_task(now_ms);
}

const app_status_t *app_get_status(void)
{
    return &g_app;
}

void app_toggle_fan_force(void)
{
    g_app.fan_forced_on ^= 1U;
    app_apply_outputs(HAL_GetTick());
}

void app_toggle_display_units(void)
{
    g_app.display_units_f ^= 1U;
    settings_set_display_units_f(g_app.display_units_f);
    settings_save();
    app_update_temperature_snapshot();
}

void app_toggle_heat_force(uint32_t now_ms)
{
    (void)now_ms;
    if (g_app.profile_active == 0U)
    {
        control_toggle_enabled();
        app_update_temperature_snapshot();
    }
}

void app_adjust_hold_target_c_x10(int16_t delta_c_x10)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->hold_target_c_x10 + delta_c_x10);

    value = app_clamp_i16(value, APP_TARGET_MIN_C_X10, APP_TARGET_MAX_C_X10);
    settings_set_hold_target_c_x10(value);
    app_update_temperature_snapshot();
}

void app_adjust_deadband_c_x10(int16_t delta_c_x10)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->control_deadband_c_x10 + delta_c_x10);

    value = app_clamp_i16(value, APP_DEADBAND_MIN_C_X10, APP_DEADBAND_MAX_C_X10);
    settings_set_control_deadband_c_x10(value);
    app_update_temperature_snapshot();
}

void app_adjust_cool_margin_c_x10(int16_t delta_c_x10)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->control_cool_margin_c_x10 + delta_c_x10);

    value = app_clamp_i16(value, APP_COOL_MARGIN_MIN_C_X10, APP_COOL_MARGIN_MAX_C_X10);
    settings_set_control_cool_margin_c_x10(value);
    app_update_temperature_snapshot();
}

void app_toggle_display_units_setting(void)
{
    g_app.display_units_f ^= 1U;
    settings_set_display_units_f(g_app.display_units_f);
    app_update_temperature_snapshot();
}

void app_adjust_left_cal_offset_c_x10(int16_t delta_c_x10)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->left_offset_c_x10 + delta_c_x10);

    value = app_clamp_i16(value, APP_CAL_OFFSET_MIN_C_X10, APP_CAL_OFFSET_MAX_C_X10);
    settings_set_left_calibration(settings->left_gain_x1000, value);
    app_update_temperature_snapshot();
}

void app_adjust_left_cal_gain_x1000(int16_t delta_x1000)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->left_gain_x1000 + delta_x1000);

    value = app_clamp_i16(value, APP_LEFT_GAIN_MIN_X1000, APP_LEFT_GAIN_MAX_X1000);
    settings_set_left_calibration(value, settings->left_offset_c_x10);
    app_update_temperature_snapshot();
}

void app_adjust_right_cal_offset_c_x10(int16_t delta_c_x10)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->right_offset_c_x10 + delta_c_x10);

    value = app_clamp_i16(value, APP_CAL_OFFSET_MIN_C_X10, APP_CAL_OFFSET_MAX_C_X10);
    settings_set_right_calibration(settings->right_gain_x1000, value);
    app_update_temperature_snapshot();
}

void app_adjust_right_cal_gain_x1000(int16_t delta_x1000)
{
    const settings_data_t *settings = settings_get();
    int16_t value = (int16_t)(settings->right_gain_x1000 + delta_x1000);

    value = app_clamp_i16(value, APP_RIGHT_GAIN_MIN_X1000, APP_RIGHT_GAIN_MAX_X1000);
    settings_set_right_calibration(value, settings->right_offset_c_x10);
    app_update_temperature_snapshot();
}

void app_save_settings(void)
{
    settings_save();
    app_update_temperature_snapshot();
}


void app_toggle_profile(uint32_t now_ms)
{
    if (g_app.profile_active != 0U)
    {
        profiles_abort();
        control_set_target_override(0U, 0);
        control_set_enabled(0U);
    }
    else
    {
        profiles_start(now_ms);
        {
            const profile_status_t *profile = profiles_get_status();
            control_set_target_override(profile->active, profile->current_target_c_x10);
        }
        control_set_enabled(1U);
    }
    app_update_temperature_snapshot();
}

void app_toggle_profile_pause(uint32_t now_ms)
{
    if (g_app.profile_active == 0U)
    {
        return;
    }

    profiles_toggle_pause(now_ms);
    if (profiles_get_status()->paused != 0U)
    {
        control_set_enabled(0U);
    }
    else
    {
        control_set_enabled(1U);
    }
    app_update_temperature_snapshot();
}

void app_abort_profile(void)
{
    if (g_app.profile_active == 0U)
    {
        return;
    }

    profiles_abort();
    control_set_target_override(0U, 0);
    control_set_enabled(0U);
    app_update_temperature_snapshot();
}

void app_factory_reset(void)
{
    profiles_abort();
    control_set_target_override(0U, 0);
    control_set_enabled(0U);
    g_profile_was_active = 0U;
    g_app.fan_forced_on = 0U;

    settings_set_defaults();
    settings_save();

    profiles_restore_defaults();
    profiles_select_saved();

    faults_init();
    sensors_filter_reset();
    board_all_outputs_off();

    g_app.display_units_f = settings_get()->display_units_f;
    app_update_temperature_snapshot();
}
