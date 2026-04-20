#include "faults.h"

#define OVERTEMP_LIMIT_C_X10      3000
#define CJ_OVERTEMP_LIMIT_C_X10   1000
#define PROBE_RAW_MIN_VALID        0U
#define PROBE_RAW_MAX_VALID     4094U
#define CJ_MV_MIN_VALID          100U
#define CJ_MV_MAX_VALID         3200U
#define NO_RISE_TIMEOUT_MS     90000U
#define NO_RISE_DELTA_C_X10       30

static faults_status_t g_faults;
static uint32_t g_heat_start_time_ms;
static int16_t g_heat_start_temp_c_x10;
static uint8_t g_heat_tracking_active;
static uint8_t g_no_rise_latched;

void faults_init(void)
{
    g_faults.active_bits = 0U;
    g_faults.critical_active = 0U;
    g_faults.no_rise_active = 0U;
    g_heat_start_time_ms = 0U;
    g_heat_start_temp_c_x10 = 0;
    g_heat_tracking_active = 0U;
    g_no_rise_latched = 0U;
}

void faults_update(const app_temps_t *temps, uint8_t heat_on, uint32_t now_ms)
{
    uint32_t bits = 0U;
    int16_t current_temp_c_x10 = temps->left_c_x10;

    if (temps->right_c_x10 > current_temp_c_x10)
    {
        current_temp_c_x10 = temps->right_c_x10;
    }

    if ((temps->left_raw < PROBE_RAW_MIN_VALID) || (temps->left_raw > PROBE_RAW_MAX_VALID))
    {
        bits |= FAULT_LEFT_SENSOR;
    }

    if ((temps->right_raw < PROBE_RAW_MIN_VALID) || (temps->right_raw > PROBE_RAW_MAX_VALID))
    {
        bits |= FAULT_RIGHT_SENSOR;
    }

    if ((temps->cj_mv < CJ_MV_MIN_VALID) || (temps->cj_mv > CJ_MV_MAX_VALID))
    {
        bits |= FAULT_CJ_SENSOR;
    }

    if ((temps->left_c_x10 > OVERTEMP_LIMIT_C_X10) ||
        (temps->right_c_x10 > OVERTEMP_LIMIT_C_X10) ||
        (temps->cj_c_x10 > CJ_OVERTEMP_LIMIT_C_X10))
    {
        bits |= FAULT_OVERTEMP;
    }

    if ((heat_on != 0U) && (g_no_rise_latched == 0U))
    {
        if (g_heat_tracking_active == 0U)
        {
            g_heat_tracking_active = 1U;
            g_heat_start_time_ms = now_ms;
            g_heat_start_temp_c_x10 = current_temp_c_x10;
        }
        else if ((now_ms - g_heat_start_time_ms) >= NO_RISE_TIMEOUT_MS)
        {
            if ((current_temp_c_x10 - g_heat_start_temp_c_x10) < NO_RISE_DELTA_C_X10)
            {
                g_no_rise_latched = 1U;
            }
            else
            {
                g_heat_start_time_ms = now_ms;
                g_heat_start_temp_c_x10 = current_temp_c_x10;
            }
        }
    }
    else if (heat_on == 0U)
    {
        g_heat_tracking_active = 0U;
    }

    if (g_no_rise_latched != 0U)
    {
        bits |= FAULT_NO_TEMP_RISE;
    }

    g_faults.active_bits = bits;
    g_faults.no_rise_active = (g_no_rise_latched != 0U) ? 1U : 0U;
    g_faults.critical_active = (bits != 0U) ? 1U : 0U;
}

const faults_status_t *faults_get_status(void)
{
    return &g_faults;
}
