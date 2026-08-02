#ifndef BZM_OVERHEAT_RECOVERY_H
#define BZM_OVERHEAT_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

/* Match the upstream ESP-Miner recovery policy. The Bonanza controller
 * applies these gates only after its stricter OFF_SAFE contract is proven. */
#define BZM_OVERHEAT_MIN_COOLING_MS 30000U
#define BZM_OVERHEAT_SAMPLE_PERIOD_MS 5000U
#define BZM_OVERHEAT_VREG_SAFE_C 95.0f
#define BZM_OVERHEAT_ASIC_SAFE_C 45.0f
#define BZM_OVERHEAT_VOLTAGE_REDUCTION_MV 100U
#define BZM_OVERHEAT_FREQUENCY_REDUCTION_MHZ 100.0f

typedef enum
{
    BZM_OVERHEAT_RECOVERY_INACTIVE = 0,
    BZM_OVERHEAT_RECOVERY_WAIT_OFF_SAFE,
    BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY,
    BZM_OVERHEAT_RECOVERY_WAIT_ASIC,
    BZM_OVERHEAT_RECOVERY_WAIT_MINIMUM,
    BZM_OVERHEAT_RECOVERY_WAIT_VREG,
    BZM_OVERHEAT_RECOVERY_READY,
    BZM_OVERHEAT_RECOVERY_INVALID,
} bzm_overheat_recovery_status_t;

typedef struct
{
    bool active;
    uint64_t started_at_ms;
    uint64_t cooling_since_ms;
    uint16_t original_voltage_mv;
    float original_frequency_mhz;
} bzm_overheat_recovery_t;

bool bzm_overheat_recovery_begin(bzm_overheat_recovery_t *recovery,
                                 uint64_t now_ms,
                                 uint16_t original_voltage_mv,
                                 float original_frequency_mhz);

bzm_overheat_recovery_status_t bzm_overheat_recovery_evaluate(
    bzm_overheat_recovery_t *recovery, uint64_t now_ms,
    bool off_safe_verified, bool vreg_temperature_available,
    float vreg_temperature_c, bool asic_temperature_available,
    float asic_temperature_c);

bool bzm_overheat_recovery_reduced_targets(
    const bzm_overheat_recovery_t *recovery, uint16_t minimum_voltage_mv,
    float minimum_frequency_mhz, uint16_t *reduced_voltage_mv,
    float *reduced_frequency_mhz);

const char *bzm_overheat_recovery_status_name(
    bzm_overheat_recovery_status_t status);

#endif /* BZM_OVERHEAT_RECOVERY_H */
