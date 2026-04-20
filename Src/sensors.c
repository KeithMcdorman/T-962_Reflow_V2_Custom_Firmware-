#include "sensors.h"
#include "main.h"

#define SENSORS_FILTER_SHIFT 3U
#define SENSORS_FILTER_SCALE (1U << SENSORS_FILTER_SHIFT)
#define SENSORS_FILTER_CHANNEL_COUNT 3U

typedef struct
{
  uint32_t channel;
  uint32_t filtered_qn;
  uint8_t initialized;
} sensor_filter_state_t;

extern ADC_HandleTypeDef hadc1;

static sensor_filter_state_t g_sensor_filters[SENSORS_FILTER_CHANNEL_COUNT] = {
    { ADC_CHANNEL_0, 0U, 0U },
    { ADC_CHANNEL_2, 0U, 0U },
    { ADC_CHANNEL_6, 0U, 0U }
};

static sensor_filter_state_t *sensors_get_filter_state(uint32_t channel)
{
  uint32_t i;

  for (i = 0U; i < SENSORS_FILTER_CHANNEL_COUNT; i++)
  {
    if (g_sensor_filters[i].channel == channel)
    {
      return &g_sensor_filters[i];
    }
  }

  return (sensor_filter_state_t *)0;
}

void sensors_analog_pins_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_6;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void sensors_filter_reset(void)
{
  uint32_t i;

  for (i = 0U; i < SENSORS_FILTER_CHANNEL_COUNT; i++)
  {
    g_sensor_filters[i].filtered_qn = 0U;
    g_sensor_filters[i].initialized = 0U;
  }
}

uint16_t sensors_adc_read_channel_raw(uint32_t channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  sConfig.Channel = channel;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_PollForConversion(&hadc1, 20U) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc1);
    return 0U;
  }

  {
    uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return raw;
  }
}

void sensors_adc_sample_channel(uint32_t channel, sensor_adc_sample_t *sample)
{
  sensor_filter_state_t *state;
  uint16_t raw;
  uint16_t filtered_raw;

  if (sample == (sensor_adc_sample_t *)0)
  {
    return;
  }

  raw = sensors_adc_read_channel_raw(channel);
  state = sensors_get_filter_state(channel);

  if (state == (sensor_filter_state_t *)0)
  {
    filtered_raw = raw;
  }
  else if (state->initialized == 0U)
  {
    state->filtered_qn = ((uint32_t)raw << SENSORS_FILTER_SHIFT);
    state->initialized = 1U;
    filtered_raw = raw;
  }
  else
  {
    uint32_t current_filtered = (state->filtered_qn + (SENSORS_FILTER_SCALE / 2U)) >> SENSORS_FILTER_SHIFT;
    state->filtered_qn += (uint32_t)raw;
    state->filtered_qn -= current_filtered;
    filtered_raw = (uint16_t)((state->filtered_qn + (SENSORS_FILTER_SCALE / 2U)) >> SENSORS_FILTER_SHIFT);
  }

  sample->raw = raw;
  sample->filtered_raw = filtered_raw;
  sample->raw_mv = sensors_adc_raw_to_millivolts(raw);
  sample->filtered_mv = sensors_adc_raw_to_millivolts(filtered_raw);
}

uint32_t sensors_adc_raw_to_millivolts(uint16_t raw)
{
  return ((uint32_t)raw * 3300U) / 4095U;
}
