#ifndef BOARD_H
#define BOARD_H

#include "main.h"
#include <stdbool.h>

void board_heat_set(bool on);
void board_fan_set(bool on);
void board_buzzer_set(bool on);
void board_all_outputs_off(void);

#endif /* BOARD_H */
