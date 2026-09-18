#ifndef BOARD_POWER_H
#define BOARD_POWER_H
#include "esp_err.h"
#include "power_policy.h"
typedef struct GlobalState GlobalState;
esp_err_t BoardPower_init(GlobalState *state, power_operations_t *operations);
power_sample_t BoardPower_sample(GlobalState *state);
#endif
