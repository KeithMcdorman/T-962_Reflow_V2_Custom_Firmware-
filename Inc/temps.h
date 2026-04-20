#ifndef TEMPS_H
#define TEMPS_H

#include <stdint.h>

int16_t temps_cold_junction_mv_to_temp_c_x10(uint32_t mv);
int16_t temps_left_probe_mv_to_temp_c_x10(uint32_t mv, int16_t cj_temp_c_x10);
int16_t temps_right_probe_mv_to_temp_c_x10(uint32_t mv, int16_t cj_temp_c_x10);
int16_t temps_c_to_f_x10(int16_t cx10);

#endif /* TEMPS_H */
