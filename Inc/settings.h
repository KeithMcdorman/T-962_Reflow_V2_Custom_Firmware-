#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

/* EEPROM layout for 1 KByte external EEPROM
 * 0x000-0x03F : settings copy A
 * 0x040-0x07F : settings copy B
 * 0x080-0x0FF : reserved for future settings expansion
 * 0x100-0x17F : profile copy A
 * 0x180-0x1FF : profile copy B
 */

#define SETTINGS_VERSION      0x0004U
#define SETTINGS_SLOT_A_ADDR  0x0000U
#define SETTINGS_SLOT_B_ADDR  0x0040U
#define SETTINGS_SLOT_SIZE    0x0040U

typedef struct
{
  int32_t left_gain_x1000;
  int16_t left_offset_c_x10;
  int32_t right_gain_x1000;
  int16_t right_offset_c_x10;
  int16_t hold_target_c_x10;
  int16_t control_deadband_c_x10;
  int16_t control_cool_margin_c_x10;
  uint8_t display_units_f;
  uint8_t reserved[7];
  int16_t calib_left_offset_c_x10;
  int16_t calib_right_offset_c_x10;
} settings_data_t;

void settings_init(void);
const settings_data_t *settings_get(void);

void settings_set_defaults(void);
void settings_set_display_units_f(uint8_t display_units_f);
void settings_set_left_calibration(int32_t gain_x1000, int16_t offset_c_x10);
void settings_set_right_calibration(int32_t gain_x1000, int16_t offset_c_x10);
void settings_set_hold_target_c_x10(int16_t target_c_x10);
void settings_set_control_deadband_c_x10(int16_t deadband_c_x10);
void settings_set_control_cool_margin_c_x10(int16_t cool_margin_c_x10);

void settings_save(void);

#endif
