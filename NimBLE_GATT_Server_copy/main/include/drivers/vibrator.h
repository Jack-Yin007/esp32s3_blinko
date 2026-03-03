#ifndef VIBRATOR_H
#define VIBRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    NONE_VIBRATION,
    SHORT_VIBRATION,
    DOUBLE_VIBRATION,
    HIGH_FRENQUENCY_VIBRATION
}vibrator_state_e;
#define  SINGLE_VIBRATION_TIME     250 //uint ms


void vibrator_init(void);
void switch_vibration_type(vibrator_state_e state);

#ifdef __cplusplus
}
#endif

#endif // VIBRATOR_H