#include "board.h"

#define HEAT_ACTIVE_LEVEL   GPIO_PIN_SET
#define FAN_ACTIVE_LEVEL    GPIO_PIN_SET
#define BUZZER_ACTIVE_LEVEL GPIO_PIN_SET

static inline GPIO_PinState board_pin_inactive(GPIO_PinState active)
{
  return (active == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void board_heat_set(bool on)
{
  HAL_GPIO_WritePin(HEAT_CTRL_GPIO_Port, HEAT_CTRL_Pin,
                    on ? HEAT_ACTIVE_LEVEL : board_pin_inactive(HEAT_ACTIVE_LEVEL));
}

void board_fan_set(bool on)
{
  HAL_GPIO_WritePin(FAN_CTRL_GPIO_Port, FAN_CTRL_Pin,
                    on ? FAN_ACTIVE_LEVEL : board_pin_inactive(FAN_ACTIVE_LEVEL));
}

void board_buzzer_set(bool on)
{
  HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin,
                    on ? BUZZER_ACTIVE_LEVEL : board_pin_inactive(BUZZER_ACTIVE_LEVEL));
}

void board_all_outputs_off(void)
{
  board_heat_set(false);
  board_fan_set(false);
  board_buzzer_set(false);
}
