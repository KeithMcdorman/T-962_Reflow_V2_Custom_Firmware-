#include "main.h"
#include "settings.h"
#include <stdint.h>

#define CJ_PULLUP_OHMS 5100U

int16_t temps_cold_junction_mv_to_temp_c_x10(uint32_t mv)
{
  static const struct { int16_t temp_c_x10; uint32_t ohms; } table[] = {
    { -200, 49531 }, { -100, 27375 }, {    0, 15802 }, {  100, 9482 },
    {  200,  5892 }, {  300,  3777 }, {  400,  2492 }, {  500, 1686 },
    {  600,  1168 }, {  700,   827 }, {  800,   597 }, {  900,  439 },
    { 1000,   328 }, { 1100,   249 }, { 1200,   191 }, { 1300,  149 },
    { 1400,   118 }, { 1500,    94 }
  };

  if (mv == 0U || mv >= 3299U)
  {
    return 0;
  }

  {
    uint32_t r_ntc = (CJ_PULLUP_OHMS * mv) / (3300U - mv);
    uint32_t i;

    if (r_ntc >= table[0].ohms)
    {
      return table[0].temp_c_x10;
    }

    for (i = 0U; i < (sizeof(table) / sizeof(table[0])) - 1U; i++)
    {
      if ((r_ntc <= table[i].ohms) && (r_ntc >= table[i + 1U].ohms))
      {
        int32_t r_hi = (int32_t)table[i].ohms;
        int32_t r_lo = (int32_t)table[i + 1U].ohms;
        int32_t t_hi = (int32_t)table[i].temp_c_x10;
        int32_t t_lo = (int32_t)table[i + 1U].temp_c_x10;
        return (int16_t)(t_hi + ((int32_t)(r_ntc - r_hi) * (t_lo - t_hi)) / (r_lo - r_hi));
      }
    }

    return table[(sizeof(table) / sizeof(table[0])) - 1U].temp_c_x10;
  }
}

static int16_t temps_apply_calibration(int16_t temp_c_x10, int32_t gain_x1000, int32_t offset_c_x10)
{
  int32_t calibrated_c_x10 = ((int32_t)temp_c_x10 * gain_x1000) / 1000 + offset_c_x10;

  if (calibrated_c_x10 < -400)
  {
    calibrated_c_x10 = -400;
  }
  if (calibrated_c_x10 > 3500)
  {
    calibrated_c_x10 = 3500;
  }

  return (int16_t)calibrated_c_x10;
}

static int16_t temps_probe_mv_to_temp_c_x10_raw(uint32_t mv, int16_t cj_temp_c_x10)
{
  int32_t temp_c_x10 = (int32_t)cj_temp_c_x10 + (int32_t)((1814UL * mv + 500UL) / 1000UL);

  if (temp_c_x10 < -400)
  {
    temp_c_x10 = -400;
  }
  if (temp_c_x10 > 3500)
  {
    temp_c_x10 = 3500;
  }

  return (int16_t)temp_c_x10;
}

int16_t temps_left_probe_mv_to_temp_c_x10(uint32_t mv, int16_t cj_temp_c_x10)
{
  const settings_data_t *settings = settings_get();
  int16_t raw_c_x10 = temps_probe_mv_to_temp_c_x10_raw(mv, cj_temp_c_x10);
  return temps_apply_calibration(raw_c_x10, settings->left_gain_x1000, settings->left_offset_c_x10);
}

int16_t temps_right_probe_mv_to_temp_c_x10(uint32_t mv, int16_t cj_temp_c_x10)
{
  const settings_data_t *settings = settings_get();
  int16_t raw_c_x10 = temps_probe_mv_to_temp_c_x10_raw(mv, cj_temp_c_x10);
  return temps_apply_calibration(raw_c_x10, settings->right_gain_x1000, settings->right_offset_c_x10);
}

int16_t temps_c_to_f_x10(int16_t cx10)
{
  return (int16_t)(((int32_t)cx10 * 9 + 25) / 5 + 320);
}
