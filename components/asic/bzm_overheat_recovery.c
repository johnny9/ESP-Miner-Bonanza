#include "bzm_overheat_recovery.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

bool bzm_overheat_recovery_begin(bzm_overheat_recovery_t *recovery,
                                 uint64_t now_ms,
                                 uint16_t original_voltage_mv,
                                 float original_frequency_mhz)
{
    if (recovery == NULL || original_voltage_mv == 0 ||
        !isfinite(original_frequency_mhz) || original_frequency_mhz <= 0.0f) {
        return false;
    }
    *recovery = (bzm_overheat_recovery_t){
        .active = true,
        .started_at_ms = now_ms,
        .cooling_since_ms = now_ms,
        .original_voltage_mv = original_voltage_mv,
        .original_frequency_mhz = original_frequency_mhz,
    };
    return true;
}

bzm_overheat_recovery_status_t bzm_overheat_recovery_evaluate(
    bzm_overheat_recovery_t *recovery, uint64_t now_ms,
    bool off_safe_verified, bool vreg_temperature_available,
    float vreg_temperature_c, bool asic_temperature_available,
    float asic_temperature_c)
{
    if (recovery == NULL || !recovery->active) {
        return BZM_OVERHEAT_RECOVERY_INACTIVE;
    }
    if (now_ms < recovery->started_at_ms ||
        now_ms < recovery->cooling_since_ms) {
        return BZM_OVERHEAT_RECOVERY_INVALID;
    }
    if (!off_safe_verified) {
        return BZM_OVERHEAT_RECOVERY_WAIT_OFF_SAFE;
    }
    if (!vreg_temperature_available || !isfinite(vreg_temperature_c)) {
        return BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY;
    }
    if (asic_temperature_available) {
        if (!isfinite(asic_temperature_c)) {
            return BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY;
        }
        if (asic_temperature_c > BZM_OVERHEAT_ASIC_SAFE_C) {
            /* Upstream restarts its six-cycle proof whenever a powered-down
             * board still has a valid, hot external ASIC sensor. */
            recovery->cooling_since_ms = now_ms;
            return BZM_OVERHEAT_RECOVERY_WAIT_ASIC;
        }
    }
    if (now_ms - recovery->cooling_since_ms <
        BZM_OVERHEAT_MIN_COOLING_MS) {
        return BZM_OVERHEAT_RECOVERY_WAIT_MINIMUM;
    }
    if (vreg_temperature_c > BZM_OVERHEAT_VREG_SAFE_C) {
        return BZM_OVERHEAT_RECOVERY_WAIT_VREG;
    }
    return BZM_OVERHEAT_RECOVERY_READY;
}

bool bzm_overheat_recovery_reduced_targets(
    const bzm_overheat_recovery_t *recovery, uint16_t minimum_voltage_mv,
    float minimum_frequency_mhz, uint16_t *reduced_voltage_mv,
    float *reduced_frequency_mhz)
{
    if (recovery == NULL || !recovery->active ||
        reduced_voltage_mv == NULL || reduced_frequency_mhz == NULL ||
        minimum_voltage_mv == 0 || !isfinite(minimum_frequency_mhz) ||
        minimum_frequency_mhz <= 0.0f ||
        recovery->original_voltage_mv < minimum_voltage_mv ||
        !isfinite(recovery->original_frequency_mhz) ||
        recovery->original_frequency_mhz < minimum_frequency_mhz) {
        return false;
    }

    *reduced_voltage_mv =
        recovery->original_voltage_mv >
                minimum_voltage_mv + BZM_OVERHEAT_VOLTAGE_REDUCTION_MV
            ? recovery->original_voltage_mv -
                  BZM_OVERHEAT_VOLTAGE_REDUCTION_MV
            : minimum_voltage_mv;
    *reduced_frequency_mhz =
        recovery->original_frequency_mhz >
                minimum_frequency_mhz +
                    BZM_OVERHEAT_FREQUENCY_REDUCTION_MHZ
            ? recovery->original_frequency_mhz -
                  BZM_OVERHEAT_FREQUENCY_REDUCTION_MHZ
            : minimum_frequency_mhz;
    return true;
}

const char *bzm_overheat_recovery_status_name(
    bzm_overheat_recovery_status_t status)
{
    switch (status) {
    case BZM_OVERHEAT_RECOVERY_INACTIVE:
        return "INACTIVE";
    case BZM_OVERHEAT_RECOVERY_WAIT_OFF_SAFE:
        return "WAIT_OFF_SAFE";
    case BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY:
        return "WAIT_TELEMETRY";
    case BZM_OVERHEAT_RECOVERY_WAIT_ASIC:
        return "WAIT_ASIC";
    case BZM_OVERHEAT_RECOVERY_WAIT_MINIMUM:
        return "WAIT_MINIMUM";
    case BZM_OVERHEAT_RECOVERY_WAIT_VREG:
        return "WAIT_VREG";
    case BZM_OVERHEAT_RECOVERY_READY:
        return "READY";
    case BZM_OVERHEAT_RECOVERY_INVALID:
        return "INVALID";
    default:
        return "UNKNOWN";
    }
}
