#ifndef POWER_MANAGEMENT_TASK_H_
#define POWER_MANAGEMENT_TASK_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "power_policy.h"

typedef struct GlobalState GlobalState;

typedef struct
{
    float fan_perc;
    uint16_t fan_rpm;
    uint16_t fan2_rpm;
    float chip_temp_avg;
    float chip_temp2_avg;
    float vr_temp;
    float voltage;
    float frequency_value;
    float actual_frequency;    
    float expected_hashrate;
    float power;
    float current;
    float core_voltage;
} PowerManagementModule;

void POWER_MANAGEMENT_init_frequency(GlobalState * GLOBAL_STATE);

void POWER_MANAGEMENT_task(void * pvParameters);

typedef enum { POWER_REQUEST_PAUSE, POWER_REQUEST_RESUME,
               POWER_REQUEST_ACQUIRE, POWER_REQUEST_RELEASE } power_request_kind_t;
/* Completion is bounded. A timeout revokes work and cancels a queued start;
 * requests own their storage until both caller and power task release it. */
bool POWER_MANAGEMENT_request(power_request_kind_t kind, power_owner_t owner,
                              uint32_t timeout_ms);
esp_err_t POWER_MANAGEMENT_init(GlobalState *state);
void POWER_MANAGEMENT_set_ready(void);
bool POWER_MANAGEMENT_wait_started(uint32_t timeout_ms);
bool POWER_MANAGEMENT_pause(void);
bool POWER_MANAGEMENT_resume(void);
bool POWER_MANAGEMENT_acquire_maintenance(power_owner_t owner);
bool POWER_MANAGEMENT_release_maintenance(power_owner_t owner);
bool POWER_MANAGEMENT_prepare_restart(void);
bool POWER_MANAGEMENT_stop_requested(void);
bool POWER_MANAGEMENT_fan_control_allowed(void);
bool POWER_MANAGEMENT_in_maintenance(void);
/* Serialize fan/query I/O with lifecycle operations and exclude maintenance.
 * This lock does not authorize callers to change board power or clocks. */
bool POWER_MANAGEMENT_board_io_begin(void);
void POWER_MANAGEMENT_board_io_end(void);
bool POWER_MANAGEMENT_overheat_recovery_active(void);
void POWER_MANAGEMENT_settings_changed(void);
void POWER_MANAGEMENT_overheat_mode_changed(bool enabled);

#endif
