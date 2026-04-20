#include "ui.h"
#include "app.h"
#include "control.h"
#include "sensors.h"
#include "board.h"
#include "faults.h"
#include "profiles.h"
#include "settings.h"
#include "temps.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint8_t stable_pressed;
  uint8_t last_sample_pressed;
  uint32_t last_change_ms;
  uint32_t last_repeat_ms;
} button_debounce_t;

typedef enum
{
  UI_PAGE_HOME = 0,
  UI_PAGE_BUILTIN_MENU,
  UI_PAGE_CUSTOM_MENU,
  UI_PAGE_CUSTOM_EDIT_MENU,
  UI_PAGE_RUN,
  UI_PAGE_PROFILE,
  UI_PAGE_PROFILE_STATUS,
  UI_PAGE_SERVICE,
  UI_PAGE_DIAG,
  UI_PAGE_CALIB,
  UI_PAGE_CAL_LEFT_OFFSET,
  UI_PAGE_CAL_LEFT_GAIN,
  UI_PAGE_CAL_RIGHT_OFFSET,
  UI_PAGE_CAL_RIGHT_GAIN,
  UI_PAGE_TARGET,
  UI_PAGE_DEADBAND,
  UI_PAGE_COOL_MARGIN,
  UI_PAGE_UNITS,
  UI_PAGE_CONTROL,
  UI_PAGE_FAN,
  UI_PAGE_PROFILE_STEP,
  UI_PAGE_PROFILE_TARGET,
  UI_PAGE_PROFILE_DURATION,
  UI_PAGE_PROFILE_COUNT,
  UI_PAGE_PROFILE_SAVE,
  UI_PAGE_PROFILE_DEFAULTS,
  UI_PAGE_FACTORY_RESET,
  UI_PAGE_COUNT
} ui_page_t;

typedef enum
{
  UI_HOME_BUILTIN = 0,
  UI_HOME_CUSTOM,
  UI_HOME_MANUAL,
  UI_HOME_COUNT
} ui_home_item_t;

typedef enum
{
  UI_PROFILE_BUILTIN = 0,
  UI_PROFILE_CUSTOM
} ui_profile_kind_t;

typedef enum
{
  UI_BUILTIN_SN63PB37 = 0,
  UI_BUILTIN_SN60PB40,
  UI_BUILTIN_SAC305,
  UI_BUILTIN_LOWTEMP,
  UI_BUILTIN_BACK,
  UI_BUILTIN_COUNT
} ui_builtin_item_t;

typedef enum
{
  UI_CUSTOM_RUN = 0,
  UI_CUSTOM_EDIT,
  UI_CUSTOM_BACK,
  UI_CUSTOM_COUNT
} ui_custom_item_t;

typedef enum
{
  UI_CUSTOM_EDIT_STEP = 0,
  UI_CUSTOM_EDIT_TARGET,
  UI_CUSTOM_EDIT_DURATION,
  UI_CUSTOM_EDIT_COUNT,
  UI_CUSTOM_EDIT_SAVE,
  UI_CUSTOM_EDIT_BACK,
  UI_CUSTOM_EDIT_COUNT_ITEMS
} ui_custom_edit_item_t;

typedef enum
{
  UI_RUN_ITEM_TARGET = 0,
  UI_RUN_ITEM_START,
  UI_RUN_ITEM_BACK,
  UI_RUN_ITEM_COUNT
} ui_run_item_t;

typedef enum
{
  UI_PROFILE_ITEM_START = 0,
  UI_PROFILE_ITEM_BACK,
  UI_PROFILE_ITEM_COUNT
} ui_profile_item_t;

typedef enum
{
  UI_SERVICE_DIAG = 0,
  UI_SERVICE_CAL,
  UI_SERVICE_MANUAL,
  UI_SERVICE_SETTINGS,
  UI_SERVICE_PROFILE,
  UI_SERVICE_RESET,
  UI_SERVICE_COUNT
} ui_service_item_t;

typedef enum
{
  UI_FACTORY_RESET_ITEM_RESET = 0,
  UI_FACTORY_RESET_ITEM_BACK,
  UI_FACTORY_RESET_ITEM_COUNT
} ui_factory_reset_item_t;

typedef struct
{
  char last_button[8];
  uint32_t last_lcd_update_ms;
  button_debounce_t up;
  button_debounce_t down;
  button_debounce_t ok;
  uint8_t lcd_front[8][128];
  uint8_t lcd_back[8][128];
  uint8_t page;
  uint8_t edit_mode;
  uint8_t home_item;
  uint8_t builtin_item;
  uint8_t custom_item;
  uint8_t custom_edit_item;
  uint8_t run_item;
  uint8_t profile_item;
  uint8_t service_item;
  uint8_t factory_reset_item;
  uint8_t profile_kind;
  uint8_t builtin_profile_id;
  uint8_t service_mode;
  uint8_t combo_armed;
  uint32_t combo_start_ms;
} ui_state_t;

#define UI_LCD_UPDATE_MS 250U
#define UI_DEBOUNCE_MS   20U
#define UI_SERVICE_HOLD_MS 1200U
#define UI_REPEAT_START_MS 500U
#define UI_REPEAT_RATE_MS  100U

#define LCD_CONTROLLER_A    0x01U
#define LCD_CONTROLLER_B    0x02U
#define LCD_CONTROLLER_BOTH (LCD_CONTROLLER_A | LCD_CONTROLLER_B)

static ui_state_t g_ui;

static bool ButtonPressed(GPIO_TypeDef *port, uint16_t pin);
static bool ButtonUpdate(button_debounce_t *btn, bool sample_pressed, uint32_t now_ms);
static bool ButtonRepeat(button_debounce_t *btn, uint32_t now_ms);
static void SafeCopy3(char *dst, const char *src);

static void Buzzer_DelayUs(uint32_t us);
static void Buzzer_ToneBurst(uint32_t freq_hz, uint32_t duration_ms);
static void Buzzer_BeepCount(uint8_t count);

static void LCD_Init(void);
static void LCD_Select(uint8_t controller_mask);
static void LCD_SetDataBus(uint8_t data);
static void LCD_PulseEnable(void);
static void LCD_Command(uint8_t cmd, uint8_t controller_mask);
static void LCD_Data(uint8_t data, uint8_t controller_mask);
static void LCD_WriteByteAt(uint8_t x, uint8_t page, uint8_t data);
static void LCD_BufferClear(uint8_t buffer[8][128]);
static uint8_t Font5x7_GetColumn(char c, uint8_t col);
static void LCD_BufferWriteChar(uint8_t buffer[8][128], uint8_t x, uint8_t page, char c);
static void LCD_BufferWriteString(uint8_t buffer[8][128], uint8_t x, uint8_t page, const char *s);
static void LCD_BufferInvertPage(uint8_t buffer[8][128], uint8_t page);
static void LCD_FlushDiff(void);

static void FormatProbeTempLine(char *dst, const char *label, int16_t temp_x10, uint8_t units_f);
static void FormatProbeFaultLine(char *dst, const char *label);
static void FormatDiagTripleLine(char *dst, char prefix, uint16_t a, uint16_t b, uint16_t c);
static void FormatDiagTempDualLine(char *dst, char left_prefix, int16_t left_x10, char right_prefix, int16_t right_x10, uint8_t units_f);
static void FormatDiagCjOvenFaultLine(char *dst, const app_status_t *status);
static void FormatDiagStatusLine(char *dst, const app_status_t *status);
static void FormatValueLine(char *dst, const char *label, int16_t value_x10, uint8_t units_f);
static void FormatSimpleStateLine(char *dst, const char *label, const char *value);
static void FormatGainLine(char *dst, const char *label, int32_t gain_x1000);
static void FormatProfileLine(char *dst, const app_status_t *status);
static void FormatProfileEditStepLine(char *dst);
static void FormatProfileEditTargetLine(char *dst, const app_status_t *status);
static void FormatProfileEditDurationLine(char *dst);
static void FormatProfileEditCountLine(char *dst);
static void FormatHeaderLine(char *dst, const char *title, const app_status_t *status);
static void FormatHomeLine(char *dst, const char *label, uint8_t selected);
static void FormatActionLine(char *dst, const char *label, uint8_t selected);
static void FormatTempOnlyLine(char *dst, const app_status_t *status);
static void FormatManualPvTargetLine(char *dst, const app_status_t *status);
static const char *UI_GetBuiltinItemLabel(uint8_t item);
static const char *UI_GetServiceItemLabel(uint8_t item);
static uint8_t UI_GetServiceItemStartPage(uint8_t item);
static uint8_t UI_GetServicePageGroup(uint8_t page);
static uint8_t UI_FindNextServicePage(uint8_t current_page);
static uint8_t UI_FindPrevServicePage(uint8_t current_page);
static void UI_Render(void);
static bool UI_IsServicePage(ui_page_t page);
static void UI_SetServiceMode(bool enabled);
static bool UI_IsNavigationLocked(const app_status_t *status);
static uint8_t UI_GetLockedPage(const app_status_t *status);

static bool ButtonPressed(GPIO_TypeDef *port, uint16_t pin)
{
  return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET);
}

static bool ButtonUpdate(button_debounce_t *btn, bool sample_pressed, uint32_t now_ms)
{
  if ((uint8_t)sample_pressed != btn->last_sample_pressed)
  {
    btn->last_sample_pressed = (uint8_t)sample_pressed;
    btn->last_change_ms = now_ms;
  }

  if ((now_ms - btn->last_change_ms) >= UI_DEBOUNCE_MS)
  {
    if (btn->stable_pressed != btn->last_sample_pressed)
    {
      btn->stable_pressed = btn->last_sample_pressed;
      if (btn->stable_pressed != 0U)
      {
        btn->last_repeat_ms = now_ms;
        return true;
      }

      btn->last_repeat_ms = 0U;
    }
  }

  return false;
}

static bool ButtonRepeat(button_debounce_t *btn, uint32_t now_ms)
{
  if ((btn->stable_pressed == 0U) || (btn->last_repeat_ms == 0U))
  {
    return false;
  }

  if ((now_ms - btn->last_change_ms) < (UI_DEBOUNCE_MS + UI_REPEAT_START_MS))
  {
    return false;
  }

  if ((now_ms - btn->last_repeat_ms) >= UI_REPEAT_RATE_MS)
  {
    btn->last_repeat_ms = now_ms;
    return true;
  }

  return false;
}

static void SafeCopy3(char *dst, const char *src)
{
  uint32_t i;
  for (i = 0U; i < 3U && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  for (; i < 3U; i++)
  {
    dst[i] = ' ';
  }
  dst[3] = '\0';
}

static void Buzzer_DelayUs(uint32_t us)
{
  uint32_t loops = us * 2U;
  while (loops-- > 0U)
  {
    __NOP();
  }
}

static void Buzzer_ToneBurst(uint32_t freq_hz, uint32_t duration_ms)
{
  uint32_t period_us;
  uint32_t half_period_us;
  uint32_t cycles;
  uint32_t i;

  if (freq_hz == 0U || duration_ms == 0U)
  {
    board_buzzer_set(false);
    return;
  }

  period_us = 1000000UL / freq_hz;
  half_period_us = period_us / 2U;
  cycles = (duration_ms * 1000UL) / period_us;

  for (i = 0U; i < cycles; i++)
  {
    board_buzzer_set(true);
    Buzzer_DelayUs(half_period_us);
    board_buzzer_set(false);
    Buzzer_DelayUs(half_period_us);
  }

  board_buzzer_set(false);
}

static void Buzzer_BeepCount(uint8_t count)
{
  uint8_t i;
  for (i = 0U; i < count; i++)
  {
    Buzzer_ToneBurst(2800U, 70U);
    HAL_Delay(90U);
  }
}

static void LCD_Select(uint8_t controller_mask)
{
  HAL_GPIO_WritePin(LCD_CSA_GPIO_Port, LCD_CSA_Pin,
                    (controller_mask & LCD_CONTROLLER_A) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_CSB_GPIO_Port, LCD_CSB_Pin,
                    (controller_mask & LCD_CONTROLLER_B) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void LCD_SetDataBus(uint8_t data)
{
  HAL_GPIO_WritePin(LCD_DB0_GPIO_Port, LCD_DB0_Pin, (data & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB1_GPIO_Port, LCD_DB1_Pin, (data & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB2_GPIO_Port, LCD_DB2_Pin, (data & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB3_GPIO_Port, LCD_DB3_Pin, (data & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB4_GPIO_Port, LCD_DB4_Pin, (data & 0x10U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB5_GPIO_Port, LCD_DB5_Pin, (data & 0x20U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB6_GPIO_Port, LCD_DB6_Pin, (data & 0x40U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DB7_GPIO_Port, LCD_DB7_Pin, (data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void LCD_PulseEnable(void)
{
  HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_SET);
  Buzzer_DelayUs(2U);
  HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);
  Buzzer_DelayUs(2U);
}

static void LCD_Command(uint8_t cmd, uint8_t controller_mask)
{
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);
  LCD_Select(controller_mask);
  LCD_SetDataBus(cmd);
  LCD_PulseEnable();
}

static void LCD_Data(uint8_t data, uint8_t controller_mask)
{
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);
  LCD_Select(controller_mask);
  LCD_SetDataBus(data);
  LCD_PulseEnable();
}

static void LCD_WriteByteAt(uint8_t x, uint8_t page, uint8_t data)
{
  uint8_t controller = (x < 64U) ? LCD_CONTROLLER_A : LCD_CONTROLLER_B;
  uint8_t local_x = (x < 64U) ? x : (uint8_t)(x - 64U);
  LCD_Command((uint8_t)(0xB8U | (page & 0x07U)), controller);
  LCD_Command((uint8_t)(0x40U | (local_x & 0x3FU)), controller);
  LCD_Data(data, controller);
}

static void LCD_BufferClear(uint8_t buffer[8][128])
{
  memset(buffer, 0, 8U * 128U);
}

static uint8_t Font5x7_GetColumn(char c, uint8_t col)
{
  static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
  };
  switch (c)
  {
    case '0' ... '9': return digits[(uint8_t)(c - '0')][col];
    case ' ': return 0x00;
    case '.': return (col == 2U) ? 0x60U : 0x00U;
    case ':': return (col == 1U || col == 3U) ? 0x24U : 0x00U;
    case '/': return (uint8_t[]){0x20,0x10,0x08,0x04,0x02}[col];
    case '-': return (col == 2U) ? 0x08U : 0x00U;
    case 'A': return (uint8_t[]){0x7E,0x11,0x11,0x11,0x7E}[col];
    case 'B': return (uint8_t[]){0x7F,0x49,0x49,0x49,0x36}[col];
    case 'C': return (uint8_t[]){0x3E,0x41,0x41,0x41,0x22}[col];
    case 'D': return (uint8_t[]){0x7F,0x41,0x41,0x22,0x1C}[col];
    case 'E': return (uint8_t[]){0x7F,0x49,0x49,0x49,0x41}[col];
    case 'F': return (uint8_t[]){0x7F,0x09,0x09,0x09,0x01}[col];
    case 'G': return (uint8_t[]){0x3E,0x41,0x49,0x49,0x7A}[col];
    case 'H': return (uint8_t[]){0x7F,0x08,0x08,0x08,0x7F}[col];
    case 'I': return (uint8_t[]){0x00,0x41,0x7F,0x41,0x00}[col];
    case 'J': return (uint8_t[]){0x20,0x40,0x41,0x3F,0x01}[col];
    case 'K': return (uint8_t[]){0x7F,0x08,0x14,0x22,0x41}[col];
    case 'L': return (uint8_t[]){0x7F,0x40,0x40,0x40,0x40}[col];
    case 'M': return (uint8_t[]){0x7F,0x02,0x04,0x02,0x7F}[col];
    case 'N': return (uint8_t[]){0x7F,0x02,0x0C,0x10,0x7F}[col];
    case 'O': return (uint8_t[]){0x3E,0x41,0x41,0x41,0x3E}[col];
    case 'P': return (uint8_t[]){0x7F,0x09,0x09,0x09,0x06}[col];
    case 'R': return (uint8_t[]){0x7F,0x09,0x19,0x29,0x46}[col];
    case 'S': return (uint8_t[]){0x46,0x49,0x49,0x49,0x31}[col];
    case 'T': return (uint8_t[]){0x01,0x01,0x7F,0x01,0x01}[col];
    case 'U': return (uint8_t[]){0x3F,0x40,0x40,0x40,0x3F}[col];
    case 'V': return (uint8_t[]){0x1F,0x20,0x40,0x20,0x1F}[col];
    default: return 0x00;
  }
}

static void LCD_BufferWriteChar(uint8_t buffer[8][128], uint8_t x, uint8_t page, char c)
{
  uint8_t col;
  if (page >= 8U || x >= 128U) return;
  for (col = 0U; col < 5U && x < 128U; col++, x++)
  {
    buffer[page][x] = Font5x7_GetColumn(c, col);
  }
  if (x < 128U) buffer[page][x] = 0x00U;
}

static void LCD_BufferWriteString(uint8_t buffer[8][128], uint8_t x, uint8_t page, const char *s)
{
  while ((*s != '\0') && (x <= 122U))
  {
    LCD_BufferWriteChar(buffer, x, page, *s++);
    x = (uint8_t)(x + 6U);
  }
}

static void LCD_BufferInvertPage(uint8_t buffer[8][128], uint8_t page)
{
  uint8_t x;

  if (page >= 8U)
  {
    return;
  }

  for (x = 0U; x < 128U; x++)
  {
    buffer[page][x] = (uint8_t)(~buffer[page][x]);
  }
}

static void LCD_FlushDiff(void)
{
  uint8_t page;
  uint8_t x;
  for (page = 0U; page < 8U; page++)
  {
    for (x = 0U; x < 128U; x++)
    {
      if (g_ui.lcd_back[page][x] != g_ui.lcd_front[page][x])
      {
        LCD_WriteByteAt(x, page, g_ui.lcd_back[page][x]);
        g_ui.lcd_front[page][x] = g_ui.lcd_back[page][x];
      }
    }
  }
}

static void LCD_Init(void)
{
  HAL_Delay(50U);
  HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
  LCD_Select(LCD_CONTROLLER_BOTH);
  HAL_Delay(10U);

  LCD_Command(0x3FU, LCD_CONTROLLER_BOTH);
  LCD_Command(0xC0U, LCD_CONTROLLER_BOTH);
  LCD_Command(0xB8U, LCD_CONTROLLER_BOTH);
  LCD_Command(0x40U, LCD_CONTROLLER_BOTH);

  memset(g_ui.lcd_front, 0xFF, sizeof(g_ui.lcd_front));
  LCD_BufferClear(g_ui.lcd_back);
  LCD_FlushDiff();
}

static void FormatProbeTempLine(char *dst, const char *label, int16_t temp_x10, uint8_t units_f)
{
  uint16_t absv;
  uint16_t whole;
  uint8_t idx = 0U;

  dst[idx++] = label[0];
  dst[idx++] = ' ';

  if (temp_x10 < 0)
  {
    dst[idx++] = '-';
    absv = (uint16_t)(-temp_x10);
  }
  else
  {
    absv = (uint16_t)temp_x10;
  }

  whole = absv / 10U;
  if (whole >= 100U)
  {
    dst[idx++] = (char)('0' + (whole / 100U) % 10U);
  }
  if (whole >= 10U)
  {
    dst[idx++] = (char)('0' + (whole / 10U) % 10U);
  }
  dst[idx++] = (char)('0' + (whole % 10U));
  dst[idx++] = units_f ? 'F' : 'C';
  dst[idx] = ' ';
}


static void FormatProbeFaultLine(char *dst, const char *label)
{
  dst[0] = label[0];
  dst[1] = ' ';
  dst[2] = 'F';
  dst[3] = 'A';
  dst[4] = 'U';
  dst[5] = 'L';
  dst[6] = 'T';
  dst[7] = '\0';
}




static void FormatDiagTripleLine(char *dst, char prefix, uint16_t a, uint16_t b, uint16_t c)
{
  (void)snprintf(dst, 20U, "%c%4u %4u %4u",
                 prefix,
                 (unsigned int)a,
                 (unsigned int)b,
                 (unsigned int)c);
}

static void FormatDiagTempDualLine(char *dst, char left_prefix, int16_t left_x10, char right_prefix, int16_t right_x10, uint8_t units_f)
{
  int16_t left_abs = (left_x10 < 0) ? (int16_t)(-left_x10) : left_x10;
  int16_t right_abs = (right_x10 < 0) ? (int16_t)(-right_x10) : right_x10;

  (void)units_f;
  (void)snprintf(dst, 20U, "%c%d.%1d %c%d.%1d",
                 left_prefix,
                 (int)(left_x10 / 10),
                 (int)(left_abs % 10),
                 right_prefix,
                 (int)(right_x10 / 10),
                 (int)(right_abs % 10));
}

static void FormatDiagCjOvenFaultLine(char *dst, const app_status_t *status)
{
  char fault_buf[8];
  char temp_buf[20];
  int16_t cj_x10 = status->temps.cj_display_x10;
  int16_t cj_abs = (cj_x10 < 0) ? (int16_t)(-cj_x10) : cj_x10;
  uint8_t copy_len;

  FormatDiagStatusLine(fault_buf, status);

  if (status->temps.oven_avg_valid != 0U)
  {
    int16_t oven_x10 = status->temps.oven_avg_display_x10;
    int16_t oven_abs = (oven_x10 < 0) ? (int16_t)(-oven_x10) : oven_x10;
    (void)snprintf(temp_buf, sizeof(temp_buf), "C%d.%1d O%d.%1d",
                   (int)(cj_x10 / 10),
                   (int)(cj_abs % 10),
                   (int)(oven_x10 / 10),
                   (int)(oven_abs % 10));
  }
  else
  {
    (void)snprintf(temp_buf, sizeof(temp_buf), "C%d.%1d O---.-",
                   (int)(cj_x10 / 10),
                   (int)(cj_abs % 10));
  }

  copy_len = (uint8_t)strlen(temp_buf);
  if (copy_len > 12U)
  {
    copy_len = 12U;
  }

  memcpy(dst, temp_buf, copy_len);
  dst[copy_len++] = ' ';

  {
    uint8_t fault_len = (uint8_t)strlen(fault_buf);
    if (fault_len > (uint8_t)(19U - copy_len))
    {
      fault_len = (uint8_t)(19U - copy_len);
    }
    memcpy(&dst[copy_len], fault_buf, fault_len);
    copy_len = (uint8_t)(copy_len + fault_len);
  }

  dst[copy_len] = ' ';
}

static void FormatDiagStatusLine(char *dst, const app_status_t *status)
{
  uint8_t pos = 0U;

  if (status->fault_bits == 0U)
  {
    dst[pos++] = 'O';
    dst[pos++] = 'K';
  }
  else
  {
    if ((status->fault_bits & FAULT_LEFT_SENSOR) != 0U) { dst[pos++] = 'L'; }
    if ((status->fault_bits & FAULT_RIGHT_SENSOR) != 0U) { dst[pos++] = 'R'; }
    if ((status->fault_bits & FAULT_CJ_SENSOR) != 0U) { dst[pos++] = 'C'; }
    if ((status->fault_bits & FAULT_OVERTEMP) != 0U) { dst[pos++] = 'O'; }
    if ((status->fault_bits & FAULT_NO_TEMP_RISE) != 0U) { dst[pos++] = 'N'; }
    if ((status->fault_bits & FAULT_STORAGE_INVALID) != 0U) { dst[pos++] = 'S'; }
  }

  dst[pos] = '\0';
}

static void FormatValueLine(char *dst, const char *label, int16_t value_x10, uint8_t units_f)
{
  uint16_t absv;
  uint16_t whole;
  uint16_t frac;

  if (value_x10 < 0)
  {
    absv = (uint16_t)(-value_x10);
    (void)snprintf(dst, 20U, "%s -%u.%u%c", label,
                   (unsigned int)(absv / 10U),
                   (unsigned int)(absv % 10U),
                   units_f ? 'F' : 'C');
  }
  else
  {
    whole = (uint16_t)value_x10 / 10U;
    frac = (uint16_t)value_x10 % 10U;
    (void)snprintf(dst, 20U, "%s %u.%u%c", label,
                   (unsigned int)whole,
                   (unsigned int)frac,
                   units_f ? 'F' : 'C');
  }
}

static void FormatSimpleStateLine(char *dst, const char *label, const char *value)
{
  (void)snprintf(dst, 20U, "%s %s", label, value);
}

static void FormatGainLine(char *dst, const char *label, int32_t gain_x1000)
{
  uint32_t abs_gain;
  uint32_t whole;
  uint32_t frac;

  if (gain_x1000 < 0)
  {
    abs_gain = (uint32_t)(-gain_x1000);
    whole = abs_gain / 1000U;
    frac = abs_gain % 1000U;
    (void)snprintf(dst, 20U, "%s -%lu.%03lu", label,
                   (unsigned long)whole,
                   (unsigned long)frac);
  }
  else
  {
    whole = (uint32_t)gain_x1000 / 1000U;
    frac = (uint32_t)gain_x1000 % 1000U;
    (void)snprintf(dst, 20U, "%s %lu.%03lu", label,
                   (unsigned long)whole,
                   (unsigned long)frac);
  }
}

void ui_init(void)
{
  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.page = (uint8_t)UI_PAGE_HOME;
  g_ui.home_item = (uint8_t)UI_HOME_BUILTIN;
  g_ui.builtin_item = (uint8_t)UI_BUILTIN_SAC305;
  g_ui.custom_item = (uint8_t)UI_CUSTOM_RUN;
  g_ui.custom_edit_item = (uint8_t)UI_CUSTOM_EDIT_STEP;
  g_ui.run_item = (uint8_t)UI_RUN_ITEM_TARGET;
  g_ui.profile_item = (uint8_t)UI_PROFILE_ITEM_START;
  g_ui.profile_kind = (uint8_t)UI_PROFILE_BUILTIN;
  g_ui.builtin_profile_id = (uint8_t)PROFILE_BUILTIN_SAC305;
  SafeCopy3(g_ui.last_button, "---");
  LCD_Init();
  Buzzer_BeepCount(2U);
}


static void FormatProfileLine(char *dst, const app_status_t *status)
{
  uint16_t target_abs;

  if (status->profile_active != 0U)
  {
    target_abs = (status->profile_target_display_x10 < 0) ? 0U : (uint16_t)status->profile_target_display_x10;
    snprintf(dst, 20U, "S%u/%u T%u.%u%c",
             (unsigned int)(status->profile_step_index + 1U),
             (unsigned int)status->profile_step_count,
             (unsigned int)(target_abs / 10U),
             (unsigned int)(target_abs % 10U),
             status->display_units_f ? 'F' : 'C');
  }
  else if (status->profile_completed != 0U)
  {
    snprintf(dst, 20U, "PROFILE DONE");
  }
  else
  {
    snprintf(dst, 20U, "PROFILE IDLE");
  }
}

static void FormatProfilePvTargetLine(char *dst, const app_status_t *status)
{
  uint16_t pv_abs = (status->profile_current_display_x10 < 0) ? 0U : (uint16_t)status->profile_current_display_x10;
  uint16_t tv_abs = (status->profile_target_display_x10 < 0) ? 0U : (uint16_t)status->profile_target_display_x10;

  snprintf(dst, 20U, "PV%u.%u%c TV%u.%u%c",
           (unsigned int)(pv_abs / 10U),
           (unsigned int)(pv_abs % 10U),
           status->display_units_f ? 'F' : 'C',
           (unsigned int)(tv_abs / 10U),
           (unsigned int)(tv_abs % 10U),
           status->display_units_f ? 'F' : 'C');
}


static void FormatManualPvTargetLine(char *dst, const app_status_t *status)
{
  uint16_t pv_abs = (status->control_temp_display_x10 < 0) ? 0U : (uint16_t)status->control_temp_display_x10;
  uint16_t tg_abs = (status->control_target_display_x10 < 0) ? 0U : (uint16_t)status->control_target_display_x10;

  (void)snprintf(dst, 20U, "PV%u.%u%c TG%u.%u%c",
                 (unsigned int)(pv_abs / 10U),
                 (unsigned int)(pv_abs % 10U),
                 status->display_units_f ? 'F' : 'C',
                 (unsigned int)(tg_abs / 10U),
                 (unsigned int)(tg_abs % 10U),
                 status->display_units_f ? 'F' : 'C');
}


static void FormatProfileEditStepLine(char *dst)
{
  uint8_t step = profiles_get_edit_step();
  const profile_t *profile = profiles_get_profile();
  (void)snprintf(dst, 20U, "STEP %u OF %u",
                 (unsigned int)(step + 1U),
                 (unsigned int)profile->step_count);
}

static void FormatProfileEditTargetLine(char *dst, const app_status_t *status)
{
  const profile_t *profile = profiles_get_profile();
  uint8_t step = profiles_get_edit_step();
  int16_t value_c_x10 = profile->steps[step].target_c_x10;
  int16_t value_disp_x10 = status->display_units_f ? temps_c_to_f_x10(value_c_x10) : value_c_x10;
  uint16_t absv = (value_disp_x10 < 0) ? 0U : (uint16_t)value_disp_x10;
  (void)snprintf(dst, 20U, "TG %u.%u%c",
                 (unsigned int)(absv / 10U),
                 (unsigned int)(absv % 10U),
                 status->display_units_f ? 'F' : 'C');
}

static void FormatProfileEditDurationLine(char *dst)
{
  const profile_t *profile = profiles_get_profile();
  uint8_t step = profiles_get_edit_step();
  uint32_t secs = profile->steps[step].duration_ms / 1000U;
  (void)snprintf(dst, 20U, "DU %lus", (unsigned long)secs);
}

static void FormatProfileEditCountLine(char *dst)
{
  const profile_t *profile = profiles_get_profile();
  (void)snprintf(dst, 20U, "COUNT %u", (unsigned int)profile->step_count);
}

static void FormatHeaderLine(char *dst, const char *title, const app_status_t *status)
{
  char temp_part[10];
  int16_t disp_x10;
  int16_t whole;
  int16_t frac;

  if (status->temps.oven_avg_valid != 0U)
  {
    disp_x10 = status->temps.oven_avg_display_x10;
    whole = disp_x10 / 10;
    frac = disp_x10 % 10;
    if (frac < 0)
    {
      frac = (int16_t)(-frac);
    }
    (void)snprintf(temp_part, sizeof(temp_part), "%3d.%1d%c",
                   whole,
                   frac,
                   status->display_units_f ? 'F' : 'C');
  }
  else
  {
    (void)snprintf(temp_part, sizeof(temp_part), "FAULT");
  }

  (void)snprintf(dst, 20U, "%-11.11s%8.8s", title, temp_part);
}

static void FormatTempOnlyLine(char *dst, const app_status_t *status)
{
  char temp_part[10];
  int16_t disp_x10;
  int16_t whole;
  int16_t frac;

  if (status->temps.oven_avg_valid != 0U)
  {
    disp_x10 = status->temps.oven_avg_display_x10;
    whole = disp_x10 / 10;
    frac = disp_x10 % 10;

    if (frac < 0)
    {
      frac = (int16_t)(-frac);
    }

    (void)snprintf(temp_part, sizeof(temp_part), "%3d.%1d%c",
                   whole,
                   frac,
                   status->display_units_f ? 'F' : 'C');
  }
  else
  {
    (void)snprintf(temp_part, sizeof(temp_part), "FAULT");
  }

  (void)snprintf(dst, 20U, "%19.19s", temp_part);
}

static void FormatHomeLine(char *dst, const char *label, uint8_t selected)
{
  uint8_t i = 0U;
  (void)selected;

  dst[0] = ' ';
  dst[1] = ' ';

  while ((i < 17U) && (label[i] != ' '))
  {
    dst[2U + i] = label[i];
    i++;
  }

  while (i < 17U)
  {
    dst[2U + i] = ' ';
    i++;
  }

  dst[19] = ' ';
}

static void FormatActionLine(char *dst, const char *label, uint8_t selected)
{
  (void)snprintf(dst, 20U, "%c %-16.16s", selected ? '>' : ' ', label);
}

static const char *UI_GetBuiltinItemLabel(uint8_t item)
{
  switch ((ui_builtin_item_t)item)
  {
    case UI_BUILTIN_SN63PB37: return "63/37 LEADED";
    case UI_BUILTIN_SN60PB40: return "60/40 LEADED";
    case UI_BUILTIN_SAC305:   return "SAC305 LF";
    case UI_BUILTIN_LOWTEMP:  return "LOWTEMP LF";
    case UI_BUILTIN_BACK:
    default:                  return "BACK";
  }
}

static const char *UI_GetServiceItemLabel(uint8_t item)
{
  switch (item)
  {
    case UI_SERVICE_DIAG: return "DIAGNOSTICS";
    case UI_SERVICE_CAL: return "CALIBRATION";
    case UI_SERVICE_MANUAL: return "MANUAL OUT";
    case UI_SERVICE_SETTINGS: return "SETTINGS";
    case UI_SERVICE_PROFILE: return "PROFILE EDIT";
    case UI_SERVICE_RESET: return "FACTORY RESET";
    default: return "SERVICE";
  }
}

static uint8_t UI_GetServiceItemStartPage(uint8_t item)
{
  switch (item)
  {
    case UI_SERVICE_DIAG: return (uint8_t)UI_PAGE_DIAG;
    case UI_SERVICE_CAL: return (uint8_t)UI_PAGE_CALIB;
    case UI_SERVICE_MANUAL: return (uint8_t)UI_PAGE_CONTROL;
    case UI_SERVICE_SETTINGS: return (uint8_t)UI_PAGE_TARGET;
    case UI_SERVICE_PROFILE: return (uint8_t)UI_PAGE_PROFILE_STEP;
    case UI_SERVICE_RESET: return (uint8_t)UI_PAGE_FACTORY_RESET;
    default: return (uint8_t)UI_PAGE_SERVICE;
  }
}

static uint8_t UI_GetServicePageGroup(uint8_t page)
{
  switch ((ui_page_t)page)
  {
    case UI_PAGE_SERVICE:
    case UI_PAGE_DIAG:
      return UI_SERVICE_DIAG;
    case UI_PAGE_CALIB:
    case UI_PAGE_CAL_LEFT_OFFSET:
    case UI_PAGE_CAL_LEFT_GAIN:
    case UI_PAGE_CAL_RIGHT_OFFSET:
    case UI_PAGE_CAL_RIGHT_GAIN:
      return UI_SERVICE_CAL;
    case UI_PAGE_CONTROL:
    case UI_PAGE_FAN:
      return UI_SERVICE_MANUAL;
    case UI_PAGE_TARGET:
    case UI_PAGE_DEADBAND:
    case UI_PAGE_COOL_MARGIN:
    case UI_PAGE_UNITS:
      return UI_SERVICE_SETTINGS;
    case UI_PAGE_PROFILE_STEP:
    case UI_PAGE_PROFILE_TARGET:
    case UI_PAGE_PROFILE_DURATION:
    case UI_PAGE_PROFILE_COUNT:
    case UI_PAGE_PROFILE_SAVE:
    case UI_PAGE_PROFILE_DEFAULTS:
      return UI_SERVICE_PROFILE;
    case UI_PAGE_FACTORY_RESET:
      return UI_SERVICE_RESET;
    default:
      return UI_SERVICE_COUNT;
  }
}

static uint8_t UI_FindNextServicePage(uint8_t current_page)
{
  uint8_t i;
  uint8_t page = current_page;
  uint8_t group = UI_GetServicePageGroup(current_page);

  if (group >= UI_SERVICE_COUNT)
  {
    return (uint8_t)UI_PAGE_SERVICE;
  }

  for (i = 0U; i < UI_PAGE_COUNT; i++)
  {
    page = (uint8_t)((page + 1U) % UI_PAGE_COUNT);
    if (UI_GetServicePageGroup(page) == group)
    {
      return page;
    }
  }

  return (uint8_t)UI_PAGE_SERVICE;
}

static uint8_t UI_FindPrevServicePage(uint8_t current_page)
{
  uint8_t i;
  uint8_t page = current_page;
  uint8_t group = UI_GetServicePageGroup(current_page);

  if (group >= UI_SERVICE_COUNT)
  {
    return (uint8_t)UI_PAGE_SERVICE;
  }

  for (i = 0U; i < UI_PAGE_COUNT; i++)
  {
    page = (page == 0U) ? (uint8_t)(UI_PAGE_COUNT - 1U) : (uint8_t)(page - 1U);
    if (UI_GetServicePageGroup(page) == group)
    {
      return page;
    }
  }

  return (uint8_t)UI_PAGE_SERVICE;
}
static void UI_Render(void)
{
  char line[20];
  const app_status_t *status = app_get_status();
  const settings_data_t *settings = settings_get();

  LCD_BufferClear(g_ui.lcd_back);

  switch ((ui_page_t)g_ui.page)
  {
    case UI_PAGE_HOME:
      FormatTempOnlyLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      FormatHomeLine(line, "BUILT-IN PROFILES", (g_ui.home_item == (uint8_t)UI_HOME_BUILTIN) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      if (g_ui.home_item == (uint8_t)UI_HOME_BUILTIN)
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 2U);
      }
      FormatHomeLine(line, "CUSTOM PROFILE", (g_ui.home_item == (uint8_t)UI_HOME_CUSTOM) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if (g_ui.home_item == (uint8_t)UI_HOME_CUSTOM)
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      }
      FormatHomeLine(line, "MANUAL TARGET", (g_ui.home_item == (uint8_t)UI_HOME_MANUAL) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if (g_ui.home_item == (uint8_t)UI_HOME_MANUAL)
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      }
      break;

    case UI_PAGE_BUILTIN_MENU:
    {
      uint8_t window_start = 0U;
      uint8_t row_item;
      uint8_t row_page;
      uint8_t row;

      FormatHeaderLine(line, "BUILT-IN", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);

      if (g_ui.builtin_item >= 2U)
      {
        if (g_ui.builtin_item >= (uint8_t)(UI_BUILTIN_COUNT - 1U))
        {
          window_start = (uint8_t)(UI_BUILTIN_COUNT - 3U);
        }
        else
        {
          window_start = (uint8_t)(g_ui.builtin_item - 1U);
        }
      }

      for (row = 0U; row < 3U; row++)
      {
        row_item = (uint8_t)(window_start + row);
        row_page = (uint8_t)(2U + (row * 2U));
        FormatHomeLine(line, UI_GetBuiltinItemLabel(row_item), (g_ui.builtin_item == row_item) ? 1U : 0U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, row_page, line);
        if (g_ui.builtin_item == row_item)
        {
          LCD_BufferInvertPage(g_ui.lcd_back, row_page);
        }
      }
      break;
    }

    case UI_PAGE_CUSTOM_MENU:
      FormatHeaderLine(line, "CUSTOM", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      FormatHomeLine(line, "RUN CUSTOM", (g_ui.custom_item == (uint8_t)UI_CUSTOM_RUN) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      if (g_ui.custom_item == (uint8_t)UI_CUSTOM_RUN) LCD_BufferInvertPage(g_ui.lcd_back, 2U);
      FormatHomeLine(line, "EDIT CUSTOM", (g_ui.custom_item == (uint8_t)UI_CUSTOM_EDIT) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if (g_ui.custom_item == (uint8_t)UI_CUSTOM_EDIT) LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      FormatHomeLine(line, "BACK", (g_ui.custom_item == (uint8_t)UI_CUSTOM_BACK) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if (g_ui.custom_item == (uint8_t)UI_CUSTOM_BACK) LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      break;

    case UI_PAGE_CUSTOM_EDIT_MENU:
      FormatHeaderLine(line, "EDIT CUSTOM", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);

      FormatHomeLine(line, "STEP SELECT", (g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_STEP) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      if (g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_STEP) LCD_BufferInvertPage(g_ui.lcd_back, 2U);

      switch (g_ui.custom_edit_item)
      {
        case UI_CUSTOM_EDIT_TARGET:
          FormatHomeLine(line, "EDIT TARGET", 1U);
          break;
        case UI_CUSTOM_EDIT_DURATION:
          FormatHomeLine(line, "EDIT TIME", 1U);
          break;
        case UI_CUSTOM_EDIT_COUNT:
          FormatHomeLine(line, "STEP COUNT", 1U);
          break;
        default:
          FormatHomeLine(line, "EDIT TARGET", 0U);
          break;
      }
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if ((g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_TARGET) ||
          (g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_DURATION) ||
          (g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_COUNT))
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      }

      switch (g_ui.custom_edit_item)
      {
        case UI_CUSTOM_EDIT_SAVE:
          FormatHomeLine(line, "SAVE CUSTOM", 1U);
          break;
        case UI_CUSTOM_EDIT_BACK:
          FormatHomeLine(line, "BACK", 1U);
          break;
        default:
          FormatHomeLine(line, "SAVE CUSTOM", 0U);
          break;
      }
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if ((g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_SAVE) ||
          (g_ui.custom_edit_item == (uint8_t)UI_CUSTOM_EDIT_BACK))
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      }
      break;

    case UI_PAGE_SERVICE:
    {
      uint8_t first_item = 0U;
      uint8_t row_item;
      uint8_t row;

      if (g_ui.service_item >= 3U)
      {
        first_item = (uint8_t)(g_ui.service_item - 3U);
      }

      for (row = 0U; row < 4U; row++)
      {
        row_item = (uint8_t)(first_item + row);
        if (row_item < UI_SERVICE_COUNT)
        {
          FormatHomeLine(line, UI_GetServiceItemLabel(row_item), (g_ui.service_item == row_item) ? 1U : 0U);
          LCD_BufferWriteString(g_ui.lcd_back, 0U, (uint8_t)(row * 2U), line);
          if (g_ui.service_item == row_item)
          {
            LCD_BufferInvertPage(g_ui.lcd_back, (uint8_t)(row * 2U));
          }
        }
      }
      break;
    }

    case UI_PAGE_FACTORY_RESET:
      FormatHeaderLine(line, "RESET", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, "RESTORE DEFAULTS?");
      FormatActionLine(line, "RESET ALL", (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_RESET) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_RESET) LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      FormatActionLine(line, "BACK", (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_BACK) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_BACK) LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      break;

    case UI_PAGE_RUN:
      FormatHeaderLine(line, "MAN HOLD", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      FormatManualPvTargetLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);

      if (g_ui.edit_mode != 0U)
      {
        LCD_BufferInvertPage(g_ui.lcd_back, 2U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, "UP/DN ADJUST");
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "OK SAVE");
      }
      else if ((status->control_enabled != 0U) || (status->heat_output_on != 0U))
      {
        g_ui.run_item = (uint8_t)UI_RUN_ITEM_START;
        FormatActionLine(line, "STOP HOLD", 1U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
        LCD_BufferInvertPage(g_ui.lcd_back, 4U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "HOLD ACTIVE");
      }
      else
      {
        if (g_ui.run_item == (uint8_t)UI_RUN_ITEM_TARGET)
        {
          LCD_BufferInvertPage(g_ui.lcd_back, 2U);
        }

        FormatActionLine(line, "START HOLD", (g_ui.run_item == (uint8_t)UI_RUN_ITEM_START) ? 1U : 0U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
        if (g_ui.run_item == (uint8_t)UI_RUN_ITEM_START) LCD_BufferInvertPage(g_ui.lcd_back, 4U);

        memset(line, ' ', 19U);
        line[19] = '\0';
        (void)memcpy(&line[15], "BACK", 4U);
        LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
        if (g_ui.run_item == (uint8_t)UI_RUN_ITEM_BACK) LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      }
      break;

    case UI_PAGE_DIAG:
      FormatDiagTripleLine(line, 'A', status->temps.left_raw, status->temps.right_raw, status->temps.cj_raw);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      FormatDiagTripleLine(line, 'F', status->temps.left_filtered_raw, status->temps.right_filtered_raw, status->temps.cj_filtered_raw);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatDiagTempDualLine(line, 'L', status->temps.left_display_x10, 'R', status->temps.right_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      FormatDiagCjOvenFaultLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      break;

    case UI_PAGE_CALIB:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CAL LIVE");
      if ((status->fault_bits & FAULT_LEFT_SENSOR) != 0U)
      {
        FormatProbeFaultLine(line, "L");
      }
      else
      {
        FormatProbeTempLine(line, "L", status->temps.left_display_x10, status->display_units_f);
      }
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      if ((status->fault_bits & FAULT_RIGHT_SENSOR) != 0U)
      {
        FormatProbeFaultLine(line, "R");
      }
      else
      {
        FormatProbeTempLine(line, "R", status->temps.right_display_x10, status->display_units_f);
      }
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "NEXT L/R CAL");
      break;

    case UI_PAGE_CAL_LEFT_OFFSET:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CAL L OFFSET");
      FormatValueLine(line, "LO", settings->left_offset_c_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProbeTempLine(line, "L", status->temps.left_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_CAL_LEFT_GAIN:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CAL L GAIN");
      FormatGainLine(line, "LG", settings->left_gain_x1000);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProbeTempLine(line, "L", status->temps.left_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_CAL_RIGHT_OFFSET:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CAL R OFFSET");
      FormatValueLine(line, "RO", settings->right_offset_c_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProbeTempLine(line, "R", status->temps.right_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_CAL_RIGHT_GAIN:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CAL R GAIN");
      FormatGainLine(line, "RG", settings->right_gain_x1000);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProbeTempLine(line, "R", status->temps.right_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_TARGET:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "SET TARGET");
      FormatValueLine(line, "TG", status->control_target_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN ADJUST" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU TARGET");
      break;

    case UI_PAGE_DEADBAND:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "SET DEADBAND");
      FormatValueLine(line, "DB", status->control_deadband_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN ADJUST" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU DBAND");
      break;

    case UI_PAGE_COOL_MARGIN:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "SET COOLMRG");
      FormatValueLine(line, "CM", status->control_cool_margin_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN ADJUST" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU COOL");
      break;

    case UI_PAGE_UNITS:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "SET UNITS");
      FormatSimpleStateLine(line, "UN", status->display_units_f ? "FAHR" : "CELS");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN TOGGLE" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU UNITS");
      break;

    case UI_PAGE_CONTROL:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "CONTROL");
      FormatSimpleStateLine(line, "CTL", status->control_enabled ? "ON" : "OFF");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatValueLine(line, "PV", status->control_temp_display_x10, status->display_units_f);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "OK TOGGLE");
      break;

    case UI_PAGE_FAN:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "MANUAL FAN");
      FormatSimpleStateLine(line, "FAN", status->fan_forced_on ? "ON" : "OFF");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatDiagStatusLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "OK TOGGLE");
      break;


    case UI_PAGE_PROFILE:
      FormatHeaderLine(line, g_ui.profile_kind == (uint8_t)UI_PROFILE_BUILTIN ? "BUILT-IN" : "CUSTOM", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      if (g_ui.profile_kind == (uint8_t)UI_PROFILE_BUILTIN)
      {
        (void)snprintf(line, 20U, "%-11.11s %uSTEP", profiles_get_builtin_name((profile_builtin_id_t)g_ui.builtin_profile_id), (unsigned int)profiles_get_profile()->step_count);
      }
      else
      {
        (void)snprintf(line, 20U, "CUSTOM %u STEP", (unsigned int)profiles_get_profile()->step_count);
      }
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatActionLine(line, status->profile_active ? "ABORT" : "START", (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_START) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_START) LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      FormatActionLine(line, "BACK", (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_BACK) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_BACK) LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      break;

    case UI_PAGE_PROFILE_STATUS:
      FormatHeaderLine(line, g_ui.profile_kind == (uint8_t)UI_PROFILE_BUILTIN ? "BUILT RUN" : "CUS RUN", status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, line);
      FormatProfileLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatActionLine(line, status->profile_paused ? "RESUME" : "PAUSE", (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_START) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      if (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_START) LCD_BufferInvertPage(g_ui.lcd_back, 4U);
      FormatActionLine(line, "ABORT", (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_BACK) ? 1U : 0U);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, line);
      if (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_BACK) LCD_BufferInvertPage(g_ui.lcd_back, 6U);
      break;

    case UI_PAGE_PROFILE_STEP:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE STEP");
      FormatProfileEditStepLine(line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN SELECT" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU PSTEP");
      break;

    case UI_PAGE_PROFILE_TARGET:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE TGT");
      FormatProfileEditTargetLine(line, status);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProfileEditStepLine(line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_PROFILE_DURATION:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE DUR");
      FormatProfileEditDurationLine(line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      FormatProfileEditStepLine(line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "UPDN ADJ OK" : "OK EDIT");
      break;

    case UI_PAGE_PROFILE_COUNT:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE CNT");
      FormatProfileEditCountLine(line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, line);
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, g_ui.edit_mode ? "UPDN ADJST" : "OK EDIT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, g_ui.edit_mode ? "OK SAVE" : "MENU PCNT");
      break;

    case UI_PAGE_PROFILE_SAVE:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE SAVE");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, "OK SAVE EEPROM");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, "STEP/TGT/DUR");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "COUNT STORED");
      break;

    case UI_PAGE_PROFILE_DEFAULTS:
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 0U, "PROFILE DFLT");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 2U, "OK RESTORE");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 4U, "LEADFREE 4STP");
      LCD_BufferWriteString(g_ui.lcd_back, 0U, 6U, "SAVE AUTO");
      break;

    default:
      break;
  }

  LCD_FlushDiff();
}


static bool UI_IsServicePage(ui_page_t page)
{
  switch (page)
  {
    case UI_PAGE_SERVICE:
    case UI_PAGE_DIAG:
    case UI_PAGE_CALIB:
    case UI_PAGE_CAL_LEFT_OFFSET:
    case UI_PAGE_CAL_LEFT_GAIN:
    case UI_PAGE_CAL_RIGHT_OFFSET:
    case UI_PAGE_CAL_RIGHT_GAIN:
    case UI_PAGE_TARGET:
    case UI_PAGE_DEADBAND:
    case UI_PAGE_COOL_MARGIN:
    case UI_PAGE_UNITS:
    case UI_PAGE_CONTROL:
    case UI_PAGE_FAN:
    case UI_PAGE_PROFILE_STEP:
    case UI_PAGE_PROFILE_TARGET:
    case UI_PAGE_PROFILE_DURATION:
    case UI_PAGE_PROFILE_COUNT:
    case UI_PAGE_PROFILE_SAVE:
    case UI_PAGE_PROFILE_DEFAULTS:
      return true;

    default:
      return false;
  }
}




static bool UI_IsNavigationLocked(const app_status_t *status)
{
  return ((status->profile_active != 0U) ||
          (status->control_enabled != 0U) ||
          (status->heat_output_on != 0U));
}

static uint8_t UI_GetLockedPage(const app_status_t *status)
{
  if (status->profile_active != 0U)
  {
    return (uint8_t)UI_PAGE_PROFILE_STATUS;
  }

  return (uint8_t)UI_PAGE_RUN;
}
static void UI_SetServiceMode(bool enabled)
{
  g_ui.service_mode = enabled ? 1U : 0U;
  g_ui.edit_mode = 0U;

  if (g_ui.service_mode != 0U)
  {
    profiles_select_saved();
    g_ui.profile_kind = (uint8_t)UI_PROFILE_CUSTOM;
    g_ui.service_item = (uint8_t)UI_SERVICE_DIAG;
    g_ui.page = (uint8_t)UI_PAGE_SERVICE;
    SafeCopy3(g_ui.last_button, "SRV");
  }
  else
  {
    if (UI_IsServicePage((ui_page_t)g_ui.page))
    {
      g_ui.page = (uint8_t)UI_PAGE_HOME;
    }
    SafeCopy3(g_ui.last_button, "RUN");
  }
}

void ui_task(uint32_t now_ms)
{
  const app_status_t *status = app_get_status();
  bool down_pressed = ButtonPressed(DOWN_GPIO_Port, DOWN_Pin);
  bool up_pressed = ButtonPressed(UP_GPIO_Port, UP_Pin);
  bool ok_pressed = ButtonPressed(OK_START_GPIO_Port, OK_START_Pin);
  bool combo_pressed = (down_pressed && up_pressed);
  bool nav_locked = UI_IsNavigationLocked(status);
  uint8_t locked_page = UI_GetLockedPage(status);

  if ((nav_locked != 0U) && (g_ui.page != locked_page) && (g_ui.edit_mode == 0U))
  {
    g_ui.page = locked_page;
    UI_Render();
  }

  if (combo_pressed && (nav_locked == 0U))
  {
    if (g_ui.combo_start_ms == 0U)
    {
      g_ui.combo_start_ms = now_ms;
      g_ui.combo_armed = 1U;
    }
    else if ((g_ui.combo_armed != 0U) && ((now_ms - g_ui.combo_start_ms) >= UI_SERVICE_HOLD_MS))
    {
      UI_SetServiceMode(g_ui.service_mode == 0U);
      g_ui.combo_armed = 0U;
      Buzzer_BeepCount(2U);
      UI_Render();
    }
  }
  else
  {
    g_ui.combo_start_ms = 0U;
    g_ui.combo_armed = 0U;
  }

  if (!combo_pressed && (ButtonUpdate(&g_ui.down, down_pressed, now_ms) || ButtonRepeat(&g_ui.down, now_ms)))
  {
    if (g_ui.edit_mode != 0U)
    {
      switch ((ui_page_t)g_ui.page)
      {
        case UI_PAGE_FACTORY_RESET:
        if (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_RESET)
        {
          profiles_abort();
          control_set_enabled(0U);
          settings_set_defaults();
          settings_save();
          profiles_restore_defaults();
          profiles_select_saved();
          faults_init();
          sensors_filter_reset();
          Buzzer_BeepCount(2U);
          NVIC_SystemReset();
        }
        else
        {
          g_ui.page = (uint8_t)UI_PAGE_SERVICE;
          SafeCopy3(g_ui.last_button, "BAK");
          Buzzer_BeepCount(1U);
          UI_Render();
        }
        break;

      case UI_PAGE_TARGET:
          app_adjust_hold_target_c_x10(-5);
          break;
        case UI_PAGE_DEADBAND:
          app_adjust_deadband_c_x10(-5);
          break;
        case UI_PAGE_COOL_MARGIN:
          app_adjust_cool_margin_c_x10(-5);
          break;
        case UI_PAGE_CAL_LEFT_OFFSET:
          app_adjust_left_cal_offset_c_x10(-1);
          break;
        case UI_PAGE_CAL_LEFT_GAIN:
          app_adjust_left_cal_gain_x1000(-1);
          break;
        case UI_PAGE_CAL_RIGHT_OFFSET:
          app_adjust_right_cal_offset_c_x10(-1);
          break;
        case UI_PAGE_CAL_RIGHT_GAIN:
          app_adjust_right_cal_gain_x1000(-1);
          break;
        case UI_PAGE_UNITS:
          app_toggle_display_units_setting();
          break;
        case UI_PAGE_PROFILE_STEP:
          profiles_set_edit_step((uint8_t)(profiles_get_edit_step() > 0U ? (profiles_get_edit_step() - 1U) : 0U));
          break;
        case UI_PAGE_PROFILE_TARGET:
          profiles_adjust_edit_target_c_x10(-5);
          break;
        case UI_PAGE_PROFILE_DURATION:
          profiles_adjust_edit_duration_s(-5);
          break;
        case UI_PAGE_PROFILE_COUNT:
          profiles_adjust_step_count(-1);
          break;
        default:
          break;
      }
      SafeCopy3(g_ui.last_button, "DEC");
      UI_Render();
    }
    else if ((nav_locked != 0U) && (g_ui.page == (uint8_t)UI_PAGE_PROFILE_STATUS))
    {
      g_ui.profile_item = (uint8_t)((g_ui.profile_item + 1U) % UI_PROFILE_ITEM_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if ((nav_locked != 0U) && (g_ui.page == (uint8_t)UI_PAGE_PROFILE_STATUS))
    {
      if (g_ui.profile_item > 0U) g_ui.profile_item--; else g_ui.profile_item = (uint8_t)(UI_PROFILE_ITEM_COUNT - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (nav_locked != 0U)
    {
      SafeCopy3(g_ui.last_button, "LCK");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_HOME)
    {
      g_ui.home_item = (uint8_t)((g_ui.home_item + 1U) % UI_HOME_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_BUILTIN_MENU)
    {
      g_ui.builtin_item = (uint8_t)((g_ui.builtin_item + 1U) % UI_BUILTIN_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_CUSTOM_MENU)
    {
      g_ui.custom_item = (uint8_t)((g_ui.custom_item + 1U) % UI_CUSTOM_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU)
    {
      g_ui.custom_edit_item = (uint8_t)((g_ui.custom_edit_item + 1U) % UI_CUSTOM_EDIT_COUNT_ITEMS);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_RUN)
    {
      g_ui.run_item = (uint8_t)((g_ui.run_item + 1U) % UI_RUN_ITEM_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_PROFILE)
    {
      g_ui.profile_item = (uint8_t)((g_ui.profile_item + 1U) % UI_PROFILE_ITEM_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_SERVICE)
    {
      g_ui.service_item = (uint8_t)((g_ui.service_item + 1U) % UI_SERVICE_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_FACTORY_RESET)
    {
      g_ui.factory_reset_item = (uint8_t)((g_ui.factory_reset_item + 1U) % UI_FACTORY_RESET_ITEM_COUNT);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
    else if (g_ui.service_mode != 0U)
    {
      g_ui.page = UI_FindNextServicePage(g_ui.page);
      SafeCopy3(g_ui.last_button, "NXT");
      UI_Render();
    }
  }

  if (!combo_pressed && (ButtonUpdate(&g_ui.up, up_pressed, now_ms) || ButtonRepeat(&g_ui.up, now_ms)))
  {
    if (g_ui.edit_mode != 0U)
    {
      switch ((ui_page_t)g_ui.page)
      {
        case UI_PAGE_FACTORY_RESET:
        if (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_RESET)
        {
          profiles_abort();
          control_set_enabled(0U);
          settings_set_defaults();
          settings_save();
          profiles_restore_defaults();
          profiles_select_saved();
          faults_init();
          sensors_filter_reset();
          Buzzer_BeepCount(2U);
          NVIC_SystemReset();
        }
        else
        {
          g_ui.page = (uint8_t)UI_PAGE_SERVICE;
          SafeCopy3(g_ui.last_button, "BAK");
          Buzzer_BeepCount(1U);
          UI_Render();
        }
        break;

      case UI_PAGE_TARGET:
          app_adjust_hold_target_c_x10(5);
          break;
        case UI_PAGE_DEADBAND:
          app_adjust_deadband_c_x10(5);
          break;
        case UI_PAGE_COOL_MARGIN:
          app_adjust_cool_margin_c_x10(5);
          break;
        case UI_PAGE_CAL_LEFT_OFFSET:
          app_adjust_left_cal_offset_c_x10(1);
          break;
        case UI_PAGE_CAL_LEFT_GAIN:
          app_adjust_left_cal_gain_x1000(1);
          break;
        case UI_PAGE_CAL_RIGHT_OFFSET:
          app_adjust_right_cal_offset_c_x10(1);
          break;
        case UI_PAGE_CAL_RIGHT_GAIN:
          app_adjust_right_cal_gain_x1000(1);
          break;
        case UI_PAGE_UNITS:
          app_toggle_display_units_setting();
          break;
        case UI_PAGE_PROFILE_STEP:
        {
          const profile_t *profile = profiles_get_profile();
          uint8_t step = profiles_get_edit_step();
          if ((step + 1U) < profile->step_count)
          {
            profiles_set_edit_step((uint8_t)(step + 1U));
          }
          break;
        }
        case UI_PAGE_PROFILE_TARGET:
          profiles_adjust_edit_target_c_x10(5);
          break;
        case UI_PAGE_PROFILE_DURATION:
          profiles_adjust_edit_duration_s(5);
          break;
        case UI_PAGE_PROFILE_COUNT:
          profiles_adjust_step_count(1);
          break;
        default:
          break;
      }
      SafeCopy3(g_ui.last_button, "INC");
      UI_Render();
    }
    else if (nav_locked != 0U)
    {
      SafeCopy3(g_ui.last_button, "LCK");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_HOME)
    {
      if (g_ui.home_item > 0U)
      {
        g_ui.home_item--;
      }
      else
      {
        g_ui.home_item = (uint8_t)(UI_HOME_COUNT - 1U);
      }
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_BUILTIN_MENU)
    {
      if (g_ui.builtin_item > 0U) g_ui.builtin_item--; else g_ui.builtin_item = (uint8_t)(UI_BUILTIN_COUNT - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_CUSTOM_MENU)
    {
      if (g_ui.custom_item > 0U) g_ui.custom_item--; else g_ui.custom_item = (uint8_t)(UI_CUSTOM_COUNT - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU)
    {
      if (g_ui.custom_edit_item > 0U) g_ui.custom_edit_item--; else g_ui.custom_edit_item = (uint8_t)(UI_CUSTOM_EDIT_COUNT_ITEMS - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_RUN)
    {
      if (g_ui.run_item > 0U) g_ui.run_item--; else g_ui.run_item = (uint8_t)(UI_RUN_ITEM_COUNT - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_PROFILE)
    {
      if (g_ui.profile_item > 0U) g_ui.profile_item--; else g_ui.profile_item = (uint8_t)(UI_PROFILE_ITEM_COUNT - 1U);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_SERVICE)
    {
      if (g_ui.service_item > 0U)
      {
        g_ui.service_item--;
      }
      else
      {
        g_ui.service_item = (uint8_t)(UI_SERVICE_COUNT - 1U);
      }
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.page == (uint8_t)UI_PAGE_FACTORY_RESET)
    {
      if (g_ui.factory_reset_item > 0U)
      {
        g_ui.factory_reset_item--;
      }
      else
      {
        g_ui.factory_reset_item = (uint8_t)(UI_FACTORY_RESET_ITEM_COUNT - 1U);
      }
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
    else if (g_ui.service_mode != 0U)
    {
      g_ui.page = UI_FindPrevServicePage(g_ui.page);
      SafeCopy3(g_ui.last_button, "PRV");
      UI_Render();
    }
  }

  if (ButtonUpdate(&g_ui.ok, ok_pressed, now_ms))
  {
    switch ((ui_page_t)g_ui.page)
    {
      case UI_PAGE_HOME:
        if (g_ui.home_item == (uint8_t)UI_HOME_BUILTIN)
        {
          g_ui.page = (uint8_t)UI_PAGE_BUILTIN_MENU;
          SafeCopy3(g_ui.last_button, "BIN");
        }
        else if (g_ui.home_item == (uint8_t)UI_HOME_CUSTOM)
        {
          g_ui.page = (uint8_t)UI_PAGE_CUSTOM_MENU;
          SafeCopy3(g_ui.last_button, "CUS");
        }
        else
        {
          g_ui.run_item = (uint8_t)UI_RUN_ITEM_TARGET;
          g_ui.page = (uint8_t)UI_PAGE_RUN;
          SafeCopy3(g_ui.last_button, "MAN");
        }
        Buzzer_BeepCount(1U);
        UI_Render();
        break;

      case UI_PAGE_BUILTIN_MENU:
        if (g_ui.builtin_item == (uint8_t)UI_BUILTIN_BACK)
        {
          g_ui.page = (uint8_t)UI_PAGE_HOME;
          SafeCopy3(g_ui.last_button, "BAK");
        }
        else
        {
          g_ui.builtin_profile_id = g_ui.builtin_item;
          profiles_select_builtin((profile_builtin_id_t)g_ui.builtin_profile_id);
          g_ui.profile_kind = (uint8_t)UI_PROFILE_BUILTIN;
          g_ui.profile_item = (uint8_t)UI_PROFILE_ITEM_START;
          g_ui.page = (uint8_t)UI_PAGE_PROFILE;
          SafeCopy3(g_ui.last_button, "SEL");
        }
        Buzzer_BeepCount(1U);
        UI_Render();
        break;

      case UI_PAGE_CUSTOM_MENU:
        if (g_ui.custom_item == (uint8_t)UI_CUSTOM_BACK)
        {
          g_ui.page = (uint8_t)UI_PAGE_HOME;
          SafeCopy3(g_ui.last_button, "BAK");
        }
        else if (g_ui.custom_item == (uint8_t)UI_CUSTOM_RUN)
        {
          profiles_select_saved();
          g_ui.profile_kind = (uint8_t)UI_PROFILE_CUSTOM;
          g_ui.profile_item = (uint8_t)UI_PROFILE_ITEM_START;
          g_ui.page = (uint8_t)UI_PAGE_PROFILE;
          SafeCopy3(g_ui.last_button, "RUN");
        }
        else
        {
          profiles_select_saved();
          g_ui.profile_kind = (uint8_t)UI_PROFILE_CUSTOM;
          g_ui.custom_edit_item = (uint8_t)UI_CUSTOM_EDIT_STEP;
          g_ui.page = (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU;
          SafeCopy3(g_ui.last_button, "EDT");
        }
        Buzzer_BeepCount(1U);
        UI_Render();
        break;

      case UI_PAGE_CUSTOM_EDIT_MENU:
        switch (g_ui.custom_edit_item)
        {
          case UI_CUSTOM_EDIT_STEP:
            g_ui.page = (uint8_t)UI_PAGE_PROFILE_STEP; SafeCopy3(g_ui.last_button, "PST"); break;
          case UI_CUSTOM_EDIT_TARGET:
            g_ui.page = (uint8_t)UI_PAGE_PROFILE_TARGET; SafeCopy3(g_ui.last_button, "PTG"); break;
          case UI_CUSTOM_EDIT_DURATION:
            g_ui.page = (uint8_t)UI_PAGE_PROFILE_DURATION; SafeCopy3(g_ui.last_button, "PDU"); break;
          case UI_CUSTOM_EDIT_COUNT:
            g_ui.page = (uint8_t)UI_PAGE_PROFILE_COUNT; SafeCopy3(g_ui.last_button, "PCN"); break;
          case UI_CUSTOM_EDIT_SAVE:
            profiles_save(); SafeCopy3(g_ui.last_button, "PSV"); break;
          default:
            g_ui.page = (uint8_t)UI_PAGE_CUSTOM_MENU; SafeCopy3(g_ui.last_button, "BAK"); break;
        }
        Buzzer_BeepCount(1U);
        UI_Render();
        break;

      case UI_PAGE_SERVICE:
        g_ui.page = UI_GetServiceItemStartPage(g_ui.service_item);
        if (g_ui.page == (uint8_t)UI_PAGE_FACTORY_RESET)
        {
          g_ui.factory_reset_item = (uint8_t)UI_FACTORY_RESET_ITEM_BACK;
        }
        SafeCopy3(g_ui.last_button, "GO ");
        Buzzer_BeepCount(1U);
        UI_Render();
        break;

      case UI_PAGE_FACTORY_RESET:
        if (g_ui.factory_reset_item == (uint8_t)UI_FACTORY_RESET_ITEM_RESET)
        {
          profiles_abort();
          control_set_enabled(0U);
          settings_set_defaults();
          settings_save();
          profiles_restore_defaults();
          profiles_select_saved();
          faults_init();
          sensors_filter_reset();
          Buzzer_BeepCount(2U);
          NVIC_SystemReset();
        }
        else
        {
          g_ui.page = (uint8_t)UI_PAGE_SERVICE;
          SafeCopy3(g_ui.last_button, "BAK");
          Buzzer_BeepCount(1U);
          UI_Render();
        }
        break;

      case UI_PAGE_TARGET:
      case UI_PAGE_DEADBAND:
      case UI_PAGE_COOL_MARGIN:
      case UI_PAGE_CAL_LEFT_OFFSET:
      case UI_PAGE_CAL_LEFT_GAIN:
      case UI_PAGE_CAL_RIGHT_OFFSET:
      case UI_PAGE_CAL_RIGHT_GAIN:
      case UI_PAGE_UNITS:
        if (g_ui.edit_mode != 0U)
        {
          g_ui.edit_mode = 0U;
          app_save_settings();
          if ((g_ui.service_mode == 0U) && ((ui_page_t)g_ui.page == UI_PAGE_TARGET))
          {
            g_ui.page = (uint8_t)UI_PAGE_RUN;
          }
          SafeCopy3(g_ui.last_button, "SAV");
          Buzzer_BeepCount(2U);
        }
        else
        {
          g_ui.edit_mode = 1U;
          SafeCopy3(g_ui.last_button, "EDT");
          Buzzer_BeepCount(1U);
        }
        UI_Render();
        break;

      case UI_PAGE_PROFILE_STEP:
      case UI_PAGE_PROFILE_TARGET:
      case UI_PAGE_PROFILE_DURATION:
      case UI_PAGE_PROFILE_COUNT:
        if (g_ui.edit_mode != 0U)
        {
          g_ui.edit_mode = 0U;
          if (g_ui.service_mode == 0U)
          {
            g_ui.page = (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU;
          }
          SafeCopy3(g_ui.last_button, "PED");
          Buzzer_BeepCount(2U);
        }
        else
        {
          g_ui.edit_mode = 1U;
          SafeCopy3(g_ui.last_button, "EDT");
          Buzzer_BeepCount(1U);
        }
        UI_Render();
        break;

      case UI_PAGE_CONTROL:
        app_toggle_heat_force(now_ms);
        SafeCopy3(g_ui.last_button, "CTL");
        Buzzer_BeepCount(2U);
        UI_Render();
        break;

      case UI_PAGE_RUN:
        if (g_ui.edit_mode != 0U)
        {
          g_ui.edit_mode = 0U;
          SafeCopy3(g_ui.last_button, "SAV");
          Buzzer_BeepCount(1U);
        }
        else if ((status->control_enabled != 0U) || (status->heat_output_on != 0U))
        {
          app_toggle_heat_force(now_ms);
          g_ui.run_item = (uint8_t)UI_RUN_ITEM_START;
          SafeCopy3(g_ui.last_button, "STP");
          Buzzer_BeepCount(2U);
        }
        else if (g_ui.run_item == (uint8_t)UI_RUN_ITEM_TARGET)
        {
          g_ui.edit_mode = 1U;
          SafeCopy3(g_ui.last_button, "TGT");
          Buzzer_BeepCount(1U);
        }
        else if (g_ui.run_item == (uint8_t)UI_RUN_ITEM_START)
        {
          app_toggle_heat_force(now_ms);
          g_ui.run_item = (uint8_t)UI_RUN_ITEM_START;
          SafeCopy3(g_ui.last_button, "GO ");
          Buzzer_BeepCount(2U);
        }
        else
        {
          g_ui.page = (uint8_t)UI_PAGE_HOME;
          SafeCopy3(g_ui.last_button, "BAK");
          Buzzer_BeepCount(1U);
        }
        UI_Render();
        break;

      case UI_PAGE_FAN:
        app_toggle_fan_force();
        SafeCopy3(g_ui.last_button, "FAN");
        Buzzer_BeepCount(2U);
        UI_Render();
        break;

      case UI_PAGE_PROFILE:
      case UI_PAGE_PROFILE_STATUS:
      {
        if (g_ui.page == (uint8_t)UI_PAGE_PROFILE_STATUS)
        {
          if (g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_START)
          {
            app_toggle_profile_pause(now_ms);
            SafeCopy3(g_ui.last_button, status->profile_paused ? "RES" : "PAU");
            Buzzer_BeepCount(2U);
          }
          else
          {
            app_abort_profile();
            g_ui.page = (uint8_t)UI_PAGE_PROFILE;
            g_ui.profile_item = (uint8_t)UI_PROFILE_ITEM_START;
            SafeCopy3(g_ui.last_button, "ABT");
            Buzzer_BeepCount(2U);
          }
        }
        else if ((g_ui.profile_item == (uint8_t)UI_PROFILE_ITEM_BACK) && (status->profile_active == 0U))
        {
          g_ui.page = (g_ui.profile_kind == (uint8_t)UI_PROFILE_BUILTIN) ? (uint8_t)UI_PAGE_BUILTIN_MENU : (uint8_t)UI_PAGE_CUSTOM_MENU;
          SafeCopy3(g_ui.last_button, "BAK");
          Buzzer_BeepCount(1U);
        }
        else
        {
          uint8_t was_active = app_get_status()->profile_active;
          app_toggle_profile(now_ms);
          if ((was_active == 0U) && (app_get_status()->profile_active != 0U))
          {
            g_ui.page = (uint8_t)UI_PAGE_PROFILE_STATUS;
            g_ui.profile_item = (uint8_t)UI_PROFILE_ITEM_START;
          }
          else if ((was_active != 0U) && (app_get_status()->profile_active == 0U))
          {
            g_ui.page = (uint8_t)UI_PAGE_PROFILE;
          }
          SafeCopy3(g_ui.last_button, "PRF");
          Buzzer_BeepCount(2U);
        }
        UI_Render();
        break;
      }

      case UI_PAGE_PROFILE_SAVE:
        profiles_save();
        if (g_ui.service_mode == 0U)
        {
          g_ui.page = (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU;
        }
        SafeCopy3(g_ui.last_button, "PSV");
        Buzzer_BeepCount(2U);
        UI_Render();
        break;

      case UI_PAGE_PROFILE_DEFAULTS:
        profiles_restore_defaults();
        if (g_ui.service_mode == 0U)
        {
          g_ui.page = (uint8_t)UI_PAGE_CUSTOM_EDIT_MENU;
        }
        SafeCopy3(g_ui.last_button, "PDF");
        Buzzer_BeepCount(2U);
        UI_Render();
        break;

      default:
        SafeCopy3(g_ui.last_button, "OK ");
        Buzzer_BeepCount(1U);
        UI_Render();
        break;
    }
  }

  if ((g_ui.page == (uint8_t)UI_PAGE_PROFILE_STATUS) && (status->profile_active == 0U) && (nav_locked == 0U))
  {
    g_ui.page = (uint8_t)UI_PAGE_PROFILE;
    UI_Render();
  }

  if ((now_ms - g_ui.last_lcd_update_ms) >= UI_LCD_UPDATE_MS)
  {
    g_ui.last_lcd_update_ms = now_ms;
    UI_Render();
  }
}
