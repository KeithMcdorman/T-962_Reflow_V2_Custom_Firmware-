#ifndef FAULTS_H
#define FAULTS_H

#include <stdint.h>
#include "app.h"

typedef enum
{
    FAULT_NONE            = 0u,
    FAULT_LEFT_SENSOR     = 1u << 0,
    FAULT_RIGHT_SENSOR    = 1u << 1,
    FAULT_CJ_SENSOR       = 1u << 2,
    FAULT_OVERTEMP        = 1u << 3,
    FAULT_NO_TEMP_RISE    = 1u << 4,
    FAULT_STORAGE_INVALID = 1u << 5,
} fault_bits_t;

typedef struct
{
    uint32_t active_bits;
    uint8_t critical_active;
    uint8_t no_rise_active;
} faults_status_t;

void faults_init(void);
void faults_update(const app_temps_t *temps, uint8_t heat_on, uint32_t now_ms);
const faults_status_t *faults_get_status(void);

#endif /* FAULTS_H */
