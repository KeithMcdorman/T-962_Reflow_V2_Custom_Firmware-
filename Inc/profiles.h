#ifndef PROFILES_H
#define PROFILES_H

#include <stdint.h>

#define PROFILE_MAX_STEPS 5U

typedef struct
{
    int16_t target_c_x10;
    uint32_t duration_ms;
} profile_step_t;

typedef struct
{
    char name[12];
    profile_step_t steps[PROFILE_MAX_STEPS];
    uint8_t step_count;
} profile_t;

typedef enum
{
    PROFILE_BUILTIN_SN63PB37 = 0,
    PROFILE_BUILTIN_SN60PB40,
    PROFILE_BUILTIN_SAC305,
    PROFILE_BUILTIN_LOWTEMP,
    PROFILE_BUILTIN_COUNT
} profile_builtin_id_t;

typedef struct
{
    uint8_t active;
    uint8_t completed;
    uint8_t paused;
    uint8_t current_step;
    uint8_t step_count;
    int16_t current_target_c_x10;
    uint32_t step_elapsed_ms;
    uint32_t step_remaining_ms;
} profile_status_t;

void profiles_init(void);
void profiles_start(uint32_t now_ms);
void profiles_abort(void);
void profiles_pause(uint32_t now_ms);
void profiles_resume(uint32_t now_ms);
void profiles_toggle_pause(uint32_t now_ms);
void profiles_update(uint32_t now_ms, uint8_t fault_critical, int16_t current_temp_c_x10);
const profile_status_t *profiles_get_status(void);
const profile_t *profiles_get_profile(void);

void profiles_set_edit_step(uint8_t step_index);
uint8_t profiles_get_edit_step(void);
void profiles_adjust_edit_target_c_x10(int16_t delta_c_x10);
void profiles_adjust_edit_duration_s(int16_t delta_s);
void profiles_adjust_step_count(int8_t delta);
void profiles_save(void);
void profiles_restore_defaults(void);
void profiles_select_default(void);
void profiles_select_builtin(profile_builtin_id_t builtin_id);
const char *profiles_get_builtin_name(profile_builtin_id_t builtin_id);
void profiles_select_saved(void);

#endif /* PROFILES_H */
