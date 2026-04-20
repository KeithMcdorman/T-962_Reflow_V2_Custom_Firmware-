#ifndef APP_H
#define APP_H

#include "main.h"
#include "profiles.h"
#include <stdint.h>

typedef struct
{
    uint16_t left_raw;
    uint16_t right_raw;
    uint16_t cj_raw;
    uint16_t left_filtered_raw;
    uint16_t right_filtered_raw;
    uint16_t cj_filtered_raw;
    uint32_t left_mv;
    uint32_t right_mv;
    uint32_t cj_mv;
    uint32_t left_filtered_mv;
    uint32_t right_filtered_mv;
    uint32_t cj_filtered_mv;
    int16_t left_c_x10;
    int16_t right_c_x10;
    int16_t cj_c_x10;
    int16_t left_display_x10;
    int16_t right_display_x10;
    int16_t cj_display_x10;
    int16_t oven_avg_c_x10;
    int16_t oven_avg_display_x10;
    uint8_t oven_avg_valid;
} app_temps_t;

typedef struct
{
    app_temps_t temps;
    uint8_t fan_forced_on;
    uint8_t heat_output_on;
    uint8_t display_units_f;
    uint32_t fault_bits;
    uint8_t fault_critical;
    uint8_t control_enabled;
    uint8_t control_heat_request;
    uint8_t control_fan_request;
    int16_t control_target_display_x10;
    int16_t control_temp_display_x10;
    int16_t control_deadband_display_x10;
    int16_t control_cool_margin_display_x10;
    uint8_t profile_active;
    uint8_t profile_paused;
    uint8_t profile_completed;
    uint8_t profile_step_index;
    uint8_t profile_step_count;
    int16_t profile_target_display_x10;
    int16_t profile_current_display_x10;
    uint32_t profile_step_elapsed_s;
    uint32_t profile_step_remaining_s;
} app_status_t;

void app_init(void);
void app_task(uint32_t now_ms);

const app_status_t *app_get_status(void);
void app_toggle_fan_force(void);
void app_toggle_display_units(void);
void app_toggle_heat_force(uint32_t now_ms);
void app_adjust_hold_target_c_x10(int16_t delta_c_x10);
void app_adjust_deadband_c_x10(int16_t delta_c_x10);
void app_adjust_cool_margin_c_x10(int16_t delta_c_x10);
void app_adjust_left_cal_offset_c_x10(int16_t delta_c_x10);
void app_adjust_left_cal_gain_x1000(int16_t delta_x1000);
void app_adjust_right_cal_offset_c_x10(int16_t delta_c_x10);
void app_adjust_right_cal_gain_x1000(int16_t delta_x1000);
void app_toggle_display_units_setting(void);
void app_save_settings(void);
void app_toggle_profile(uint32_t now_ms);
void app_toggle_profile_pause(uint32_t now_ms);
void app_abort_profile(void);
void app_factory_reset(void);

#endif /* APP_H */
