#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

typedef struct
{
    uint8_t enabled;
    int16_t target_c_x10;
    int16_t deadband_c_x10;
    int16_t cool_margin_c_x10;
    int16_t control_temp_c_x10;
    uint8_t heat_request;
    uint8_t fan_request;
    uint8_t heat_duty_percent;
} control_status_t;

void control_init(void);
void control_toggle_enabled(void);
void control_set_enabled(uint8_t enabled);
void control_set_target_override(uint8_t enabled, int16_t target_c_x10);
void control_update(int16_t left_c_x10,
                    int16_t right_c_x10,
                    uint32_t fault_bits,
                    uint8_t fault_critical);
const control_status_t *control_get_status(void);

#endif /* CONTROL_H */
