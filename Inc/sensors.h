#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

typedef struct
{
    uint16_t raw;
    uint16_t filtered_raw;
    uint32_t raw_mv;
    uint32_t filtered_mv;
} sensor_adc_sample_t;

void sensors_analog_pins_init(void);
void sensors_filter_reset(void);
uint16_t sensors_adc_read_channel_raw(uint32_t channel);
void sensors_adc_sample_channel(uint32_t channel, sensor_adc_sample_t *sample);
uint32_t sensors_adc_raw_to_millivolts(uint16_t raw);

#endif /* SENSORS_H */
