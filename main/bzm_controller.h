#ifndef BZM_CONTROLLER_H
#define BZM_CONTROLLER_H

#include <stdbool.h>

#include "bzm_supervisor.h"
#include "esp_err.h"
#include "global_state.h"

/* Bonanza production controller. These operations are invoked
 * by normal boot, OTA, and restart flows; there is no external staged control
 * surface in a production image. */
esp_err_t bzm_controller_init(GlobalState *global_state);
bool bzm_controller_active(void);
bool bzm_controller_mining_stack_ready(void);
bool bzm_controller_dispatch_allowed(void);
/* True only while the production controller owns a healthy RUNNING stage.
 * The fan task uses this to keep the bridge fan at its fail-safe 100 percent
 * during startup, shutdown, faults, and maintenance. */
bool bzm_controller_fan_control_allowed(void);

/* Pause revokes dispatch and proves the complete Bonanza OFF_SAFE contract.
 * Resume repeats the production startup at the known-good 2.8 V / 800 MHz
 * baseline before live tuning restores the saved target. Non-BZM products
 * return true without changing their generic pause state. */
bool bzm_controller_pause(void);
bool bzm_controller_resume(void);

/* Wake the live tuning worker after frequency or voltage NVS changes. */
void bzm_controller_tuning_settings_changed(void);

/* Synchronize the settings-page overheat reset. An active automatic recovery
 * retains overheat mode until its cool-down and full restart succeed; a stale
 * completed/failed mode may still be cleared like upstream. */
void bzm_controller_overheat_mode_changed(bool enabled);
bool bzm_controller_overheat_recovery_active(void);

/* Exclusive verified-safe-off ownership for production maintenance. */
bool bzm_controller_acquire_maintenance(
    bzm_supervisor_owner_t owner);
bool bzm_controller_acquire_bridge_recovery(void);
bool bzm_controller_release_maintenance(
    bzm_supervisor_owner_t owner);

/* Non-BZM products return true. Bonanza closes dispatch and proves safe-off
 * before the caller proceeds with restart. */
bool bzm_controller_prepare_restart(void);

#endif /* BZM_CONTROLLER_H */
