#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct { uint16_t voltage_mv; float frequency_mhz; } power_target_t;
typedef enum { POWER_HEALTH_OK, POWER_HEALTH_OVERHEAT, POWER_HEALTH_FAULT } power_health_t;
typedef enum { POWER_START_OK, POWER_START_FAILED, POWER_START_OVERHEAT } power_start_result_t;
typedef enum { POWER_OWNER_NONE, POWER_OWNER_OTA, POWER_OWNER_BRIDGE, POWER_OWNER_RESTART } power_owner_t;
typedef struct {
    power_health_t health;
    bool vreg_valid;
    float vreg_c;
    bool off_asic_sensor_required;
    bool asic_valid;
    float asic_c;
    char detail[160];
} power_sample_t;
typedef struct {
    void *context;
    power_start_result_t (*start)(void *context);
    /* Success means the board-specific shutdown contract was verified. */
    bool (*stop)(void *context);
    /* One bounded tuning step; waiting for work replacement is success. */
    bool (*apply)(void *context, power_target_t target);
    bool (*maintenance)(void *context, power_owner_t owner, bool acquire);
    void (*overheat)(void *context, bool enabled);
    bool (*save_target)(void *context, power_target_t target);
    bool (*cancelled)(void *context);
    uint16_t minimum_voltage_mv;
    float minimum_frequency_mhz;
} power_operations_t;
typedef struct {
    power_operations_t operations;
    bool ready;
    bool running;
    bool stopped;
    bool paused;
    bool fault;
    bool cooling;
    bool reduced_target_saved;
    bool pool_unavailable;
    power_target_t target;
    power_owner_t owner;
    uint64_t cooling_since_ms;
    power_target_t cooling_target;
} power_policy_t;

bool power_policy_init(power_policy_t *policy, power_operations_t operations);
/* Only POWER_MANAGEMENT_task calls these functions and hardware operations. */
bool power_policy_pause(power_policy_t *policy);
bool power_policy_resume(power_policy_t *policy);
bool power_policy_maintenance(power_policy_t *policy, power_owner_t owner, bool acquire);
void power_policy_step(power_policy_t *policy, uint64_t now_ms,
                       power_target_t target, power_sample_t sample,
                       bool pools_unavailable, bool hardware_fault,
                       bool persisted_overheat);

#endif
