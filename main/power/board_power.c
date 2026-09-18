#include "board_power.h"

#include <math.h>
#include "asic.h"
#include "asic_init.h"
#include "asic_reset.h"
#include "bzm_board_power.h"
#include "bzm_frequency.h"
#include "bzm_power.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "power.h"
#include "thermal.h"
#include "vcore.h"

static bool bitmain_started;
static uint16_t applied_voltage;
static float applied_frequency;

static bool bitmain_stop(void *context)
{
    GlobalState *state = context;
    if (state->ASIC_initalized) {
        state->POWER_MANAGEMENT_MODULE.frequency_value = 50;
        state->POWER_MANAGEMENT_MODULE.expected_hashrate = 0;
        ASIC_set_frequency(state);
        ASIC_set_nonce_space(state);
    }
    const bool off = VCORE_set_voltage(state, 0.0f) == ESP_OK;
    const bool reset = asic_hold_reset_low() == ESP_OK;
    state->ASIC_initalized = false;
    state->POWER_MANAGEMENT_MODULE.actual_frequency = 0;
    state->POWER_MANAGEMENT_MODULE.expected_hashrate = 0;
    applied_voltage = 0;
    applied_frequency = 50;
    if (bitmain_started) {
        vTaskDelay(pdMS_TO_TICKS(500));
        (void)uart_flush(UART_NUM_1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return off && reset;
}

static power_start_result_t bitmain_start(void *context)
{
    GlobalState *state = context;
    uint16_t voltage = state->SELF_TEST_MODULE.is_active
        ? state->DEVICE_CONFIG.family.asic.default_voltage_mv
        : nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);
    if (VCORE_set_voltage(state, voltage / 1000.0f) != ESP_OK) return POWER_START_FAILED;
    vTaskDelay(pdMS_TO_TICKS(500));
    if (bitmain_started) {
        (void)uart_flush(UART_NUM_1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    POWER_MANAGEMENT_init_frequency(state);
    uint8_t chips = asic_initialize(state,
        bitmain_started ? ASIC_INIT_RECOVERY : ASIC_INIT_COLD_BOOT,
        bitmain_started ? 2000 : 0);
    bitmain_started = true;
    applied_voltage = voltage;
    applied_frequency = state->POWER_MANAGEMENT_MODULE.frequency_value;
    return chips > 0 ? POWER_START_OK : POWER_START_FAILED;
}

static bool bitmain_apply(void *context, power_target_t target)
{
    GlobalState *state = context;
    /* Raise voltage before raising clocks; lower voltage after lowering them. */
    if (target.voltage_mv > applied_voltage) {
        if (VCORE_set_voltage(state, target.voltage_mv / 1000.0f) != ESP_OK) return false;
        applied_voltage = target.voltage_mv;
    }
    if (target.frequency_mhz != applied_frequency) {
        state->POWER_MANAGEMENT_MODULE.frequency_value = target.frequency_mhz;
        state->POWER_MANAGEMENT_MODULE.expected_hashrate = target.frequency_mhz *
            state->DEVICE_CONFIG.family.asic.small_core_count * state->DEVICE_CONFIG.family.asic_count / 1000.0f;
        ASIC_set_frequency(state);
        ASIC_set_nonce_space(state);
        applied_frequency = target.frequency_mhz;
    }
    if (target.voltage_mv < applied_voltage) {
        if (VCORE_set_voltage(state, target.voltage_mv / 1000.0f) != ESP_OK) return false;
        applied_voltage = target.voltage_mv;
    }
    return true;
}

static bool bitmain_maintenance(void *context, power_owner_t owner, bool acquire)
{
    (void)owner;
    return !acquire || bitmain_stop(context);
}

static void set_overheat(void *context, bool enabled)
{
    GlobalState *state = context;
    if (enabled) {
        nvs_config_set_bool(NVS_CONFIG_AUTO_FAN_SPEED, false);
        nvs_config_set_u16(NVS_CONFIG_MANUAL_FAN_SPEED, 100);
    }
    nvs_config_set_bool(NVS_CONFIG_OVERHEAT_MODE, enabled);
    state->SYSTEM_MODULE.overheat_mode = enabled;
}

static bool save_target(void *context, power_target_t target)
{
    GlobalState *state = context;
    if (state->DEVICE_CONFIG.bonanza_bridge) {
        bzm_frequency_target_t resolved;
        float voltage;
        if (!bzm_frequency_resolve_target(target.frequency_mhz, &resolved) ||
            !bzm_power_resolve_user_voltage(target.voltage_mv, &voltage)) return false;
        target.frequency_mhz = resolved.actual_mhz;
    }
    nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE, target.voltage_mv);
    nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY, target.frequency_mhz);
    return nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE) == target.voltage_mv &&
           fabsf(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY) - target.frequency_mhz) < 0.001f;
}

static bool cancelled(void *context)
{
    GlobalState *state = context;
    return POWER_MANAGEMENT_stop_requested() || state->SYSTEM_MODULE.pools_unavailable;
}

esp_err_t BoardPower_init(GlobalState *state, power_operations_t *operations)
{
    *operations = (power_operations_t){
        .context = state, .start = bitmain_start, .stop = bitmain_stop,
        .apply = bitmain_apply, .maintenance = bitmain_maintenance,
        .overheat = set_overheat, .save_target = save_target, .cancelled = cancelled,
        .minimum_voltage_mv = 1000, .minimum_frequency_mhz = 50,
    };
    int16_t minimum_voltage = VCORE_get_voltage_min_mv(state);
    if (minimum_voltage > 0) operations->minimum_voltage_mv = (uint16_t)minimum_voltage;
    if (state->DEVICE_CONFIG.bonanza_bridge) {
        operations->start = BZM_board_start;
        operations->stop = BZM_board_stop;
        operations->apply = BZM_board_apply;
        operations->maintenance = BZM_board_maintenance;
        operations->minimum_frequency_mhz = BZM_FREQUENCY_TARGET_MIN_MHZ;
        return BZM_board_init(state);
    }
    return ESP_OK;
}

power_sample_t BoardPower_sample(GlobalState *state)
{
    if (state->DEVICE_CONFIG.bonanza_bridge) return BZM_board_sample();
    PowerManagementModule *power = &state->POWER_MANAGEMENT_MODULE;
    power->voltage = Power_get_input_voltage(state);
    Power_get_output(state, &power->power, &power->current);
    power->core_voltage = VCORE_get_voltage_mv(state);
    power->chip_temp_avg = Thermal_get_chip_temp(state);
    power->chip_temp2_avg = Thermal_get_chip_temp2(state);
    power->vr_temp = Power_get_vreg_temp(state);
    power_sample_t result = {
        .health = POWER_HEALTH_OK,
        .vreg_valid = isfinite(power->vr_temp) && power->vr_temp >= 0,
        .vreg_c = power->vr_temp,
        .off_asic_sensor_required = state->DEVICE_CONFIG.emc_internal_temp,
        .asic_valid = power->chip_temp_avg > 0 || power->chip_temp2_avg > 0,
        .asic_c = fmaxf(power->chip_temp_avg, power->chip_temp2_avg),
    };
    if (state->ASIC_initalized &&
        (power->vr_temp > 105 || result.asic_c > THERMAL_ASIC_THROTTLE_TEMP_C))
        result.health = POWER_HEALTH_OVERHEAT;
    if (state->ASIC_initalized && VCORE_check_fault(state) != ESP_OK)
        result.health = POWER_HEALTH_FAULT;
    return result;
}
