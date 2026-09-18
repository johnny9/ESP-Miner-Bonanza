#ifndef BZM_BOARD_POWER_H
#define BZM_BOARD_POWER_H
#include "esp_err.h"
#include "power_policy.h"
typedef struct GlobalState GlobalState;
/* Hardware adapter. Lifecycle operations run only in POWER_MANAGEMENT_task. */
esp_err_t BZM_board_init(GlobalState *state);
power_start_result_t BZM_board_start(void *context);
bool BZM_board_stop(void *context);
bool BZM_board_apply(void *context, power_target_t target);
bool BZM_board_maintenance(void *context, power_owner_t owner, bool acquire);
power_sample_t BZM_board_sample(void);
#endif
