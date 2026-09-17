#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DS4432U.h"
#include "INA260.h"
#include "TPS546.h"
#include "nvs_config.h"
#include "adc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bzm_bridge.h"
#include "bzm_power.h"
#include "global_state.h"
#include "device_config.h"
#include "vcore.h"

#define GPIO_ASIC_ENABLE CONFIG_GPIO_ASIC_ENABLE
#define GPIO_PLUG_SENSE CONFIG_GPIO_PLUG_SENSE
#define GPIO_TPS546_PGOOD 11

static const char *TAG = "vcore";
static bool vcore_initialized = false;

static void log_tps546_config(const FamilyConfig *family, const TPS546_CONFIG *config)
{
    ESP_LOGI(TAG, "Selected TPS546 config for family=%s id=%d voltage_domains=%u",
             family->name, (int)family->id, (unsigned)family->voltage_domains);
    ESP_LOGI(TAG, "TPS546 init core: phase=0x%02X stack=0x%04X sync=0x%02X scale=%.3f vout=%.2f min=%.2f max=%.2f",
             config->TPS546_INIT_PHASE,
             config->TPS546_INIT_STACK_CONFIG,
             config->TPS546_INIT_SYNC_CONFIG,
             config->TPS546_INIT_SCALE_LOOP,
             config->TPS546_INIT_VOUT_COMMAND,
             config->TPS546_INIT_VOUT_MIN,
             config->TPS546_INIT_VOUT_MAX);
    ESP_LOGI(TAG, "TPS546 init VIN: on=%.2f off=%.2f uv_warn=%.2f ov_fault=%.2f",
             config->TPS546_INIT_VIN_ON,
             config->TPS546_INIT_VIN_OFF,
             config->TPS546_INIT_VIN_UV_WARN_LIMIT,
             config->TPS546_INIT_VIN_OV_FAULT_LIMIT);
    ESP_LOGI(TAG, "TPS546 init IOUT: warn=%.2f fault=%.2f",
             config->TPS546_INIT_IOUT_OC_WARN_LIMIT,
             config->TPS546_INIT_IOUT_OC_FAULT_LIMIT);
    ESP_LOGI(TAG, "TPS546 init COMPENSATION_CONFIG: %02X %02X %02X %02X %02X",
             config->TPS546_INIT_COMPENSATION_CONFIG[0],
             config->TPS546_INIT_COMPENSATION_CONFIG[1],
             config->TPS546_INIT_COMPENSATION_CONFIG[2],
             config->TPS546_INIT_COMPENSATION_CONFIG[3],
             config->TPS546_INIT_COMPENSATION_CONFIG[4]);
}

static TPS546_CONFIG get_tps546_config(const FamilyConfig * family)
{
    TPS546_CONFIG config = family->tps546_config ? *family->tps546_config : TPS546_CONFIG_DEFAULT;
    config.TPS546_INIT_PIN_DETECT_OVERRIDE = 0xFFFF;
    if (family->id == BONANZA) {
        const bzm_tps546_profile_t *profile = &BZM_TPS546_BIRDS_PROFILE;
        config.TPS546_EXTENDED_CONFIG = true;
        config.TPS546_INIT_PHASE = profile->phase;
        memcpy(config.TPS546_INIT_SMBALERT_MASK, profile->smbalert_mask,
               sizeof(config.TPS546_INIT_SMBALERT_MASK));
        config.TPS546_INIT_FREQUENCY = profile->frequency_switch_khz;
        config.TPS546_INIT_VIN_ON = profile->vin_on;
        config.TPS546_INIT_VIN_OFF = profile->vin_off;
        config.TPS546_INIT_VIN_UV_WARN_LIMIT = profile->vin_uv_warn_limit;
        config.TPS546_INIT_VIN_OV_FAULT_LIMIT = profile->vin_ov_fault_limit;
        config.TPS546_INIT_SCALE_LOOP = profile->vout_scale_loop;
        config.TPS546_INIT_VOUT_MIN = profile->vout_min;
        config.TPS546_INIT_VOUT_MAX = profile->vout_max;
        config.TPS546_INIT_VOUT_COMMAND = profile->vout_command;
        config.TPS546_INIT_IOUT_OC_WARN_LIMIT = profile->iout_oc_warn_limit;
        config.TPS546_INIT_IOUT_OC_FAULT_LIMIT = profile->iout_oc_fault_limit;
        config.TPS546_INIT_STACK_CONFIG = profile->stack_config;
        config.TPS546_INIT_SYNC_CONFIG = profile->sync_config;
        config.TPS546_INIT_INTERLEAVE = profile->interleave;
        config.TPS546_INIT_MISC_OPTIONS = profile->misc_options;
        config.TPS546_INIT_PIN_DETECT_OVERRIDE =
            profile->pin_detect_override;
        memcpy(config.TPS546_INIT_COMPENSATION_CONFIG,
               profile->compensation_config,
               sizeof(config.TPS546_INIT_COMPENSATION_CONFIG));
        config.TPS546_INIT_POWER_STAGE_CONFIG = profile->power_stage_config;
        memcpy(config.TPS546_INIT_TELEMETRY_CONFIG,
               profile->telemetry_config,
               sizeof(config.TPS546_INIT_TELEMETRY_CONFIG));
        config.TPS546_INIT_VOUT_TRIM = profile->vout_trim;
        config.TPS546_INIT_VOUT_TRANSITION_RATE =
            profile->vout_transition_rate;
        config.TPS546_INIT_IOUT_CAL_GAIN = profile->iout_cal_gain;
        config.TPS546_INIT_IOUT_CAL_OFFSET = profile->iout_cal_offset;
        config.TPS546_EXT_VOUT_MARGIN_HIGH = profile->vout_margin_high;
        config.TPS546_EXT_VOUT_MARGIN_LOW = profile->vout_margin_low;
        config.TPS546_EXT_VOUT_OV_FAULT_LIMIT =
            profile->vout_ov_fault_limit;
        config.TPS546_EXT_VOUT_OV_FAULT_RESPONSE =
            profile->vout_ov_fault_response;
        config.TPS546_EXT_VOUT_OV_WARN_LIMIT =
            profile->vout_ov_warn_limit;
        config.TPS546_EXT_VOUT_UV_WARN_LIMIT =
            profile->vout_uv_warn_limit;
        config.TPS546_EXT_VOUT_UV_FAULT_LIMIT =
            profile->vout_uv_fault_limit;
        config.TPS546_EXT_VOUT_UV_FAULT_RESPONSE =
            profile->vout_uv_fault_response;
        config.TPS546_EXT_IOUT_OC_FAULT_RESPONSE =
            profile->iout_oc_fault_response;
        config.TPS546_EXT_OT_FAULT_LIMIT = profile->ot_fault_limit;
        config.TPS546_EXT_OT_FAULT_RESPONSE = profile->ot_fault_response;
        config.TPS546_EXT_OT_WARN_LIMIT = profile->ot_warn_limit;
        config.TPS546_EXT_VIN_OV_FAULT_RESPONSE =
            profile->vin_ov_fault_response;
        config.TPS546_EXT_TON_DELAY = profile->ton_delay;
        config.TPS546_EXT_TON_RISE = profile->ton_rise;
        config.TPS546_EXT_TON_MAX_FAULT_LIMIT =
            profile->ton_max_fault_limit;
        config.TPS546_EXT_TON_MAX_FAULT_RESPONSE =
            profile->ton_max_fault_response;
        config.TPS546_EXT_TOFF_DELAY = profile->toff_delay;
        config.TPS546_EXT_TOFF_FALL = profile->toff_fall;
        return config;
    }


    if (nvs_config_has_key(NVS_CONFIG_TPS546_PHASE)) {
        config.TPS546_INIT_PHASE = (uint8_t)nvs_config_get_u16(NVS_CONFIG_TPS546_PHASE);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VIN_ON)) {
        config.TPS546_INIT_VIN_ON = nvs_config_get_float(NVS_CONFIG_TPS546_VIN_ON);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VIN_OFF)) {
        config.TPS546_INIT_VIN_OFF = nvs_config_get_float(NVS_CONFIG_TPS546_VIN_OFF);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VIN_UV_WARN)) {
        config.TPS546_INIT_VIN_UV_WARN_LIMIT = nvs_config_get_float(NVS_CONFIG_TPS546_VIN_UV_WARN);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VIN_OV_FAULT)) {
        config.TPS546_INIT_VIN_OV_FAULT_LIMIT = nvs_config_get_float(NVS_CONFIG_TPS546_VIN_OV_FAULT);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_SCALE_LOOP)) {
        config.TPS546_INIT_SCALE_LOOP = nvs_config_get_float(NVS_CONFIG_TPS546_SCALE_LOOP);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VOUT_MIN)) {
        config.TPS546_INIT_VOUT_MIN = nvs_config_get_float(NVS_CONFIG_TPS546_VOUT_MIN);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VOUT_MAX)) {
        config.TPS546_INIT_VOUT_MAX = nvs_config_get_float(NVS_CONFIG_TPS546_VOUT_MAX);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_VOUT_COMMAND)) {
        config.TPS546_INIT_VOUT_COMMAND = nvs_config_get_float(NVS_CONFIG_TPS546_VOUT_COMMAND);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_IOUT_OC_WARN)) {
        config.TPS546_INIT_IOUT_OC_WARN_LIMIT = nvs_config_get_float(NVS_CONFIG_TPS546_IOUT_OC_WARN);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_IOUT_OC_FAULT)) {
        config.TPS546_INIT_IOUT_OC_FAULT_LIMIT = nvs_config_get_float(NVS_CONFIG_TPS546_IOUT_OC_FAULT);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_STACK_CONFIG)) {
        config.TPS546_INIT_STACK_CONFIG = nvs_config_get_u16(NVS_CONFIG_TPS546_STACK_CONFIG);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_SYNC_CONFIG)) {
        config.TPS546_INIT_SYNC_CONFIG = (uint8_t)nvs_config_get_u16(NVS_CONFIG_TPS546_SYNC_CONFIG);
    }
    if (nvs_config_has_key(NVS_CONFIG_TPS546_FREQUENCY)) {
        config.TPS546_INIT_FREQUENCY = nvs_config_get_u16(NVS_CONFIG_TPS546_FREQUENCY);
    }

    log_tps546_config(family, &config);
    return config;
}

static void configure_asic_power_enable(GlobalState * GLOBAL_STATE)
{
    if (!(GLOBAL_STATE->DEVICE_CONFIG.plug_sense || GLOBAL_STATE->DEVICE_CONFIG.asic_enable)) {
        return;
    }

    bool enable_power = GLOBAL_STATE->DEVICE_CONFIG.asic_enable;

    if (GLOBAL_STATE->DEVICE_CONFIG.plug_sense) {
        gpio_config_t barrel_jack_conf = {
            .pin_bit_mask = (1ULL << GPIO_PLUG_SENSE),
            .mode = GPIO_MODE_INPUT,
        };
        gpio_config(&barrel_jack_conf);
        enable_power = gpio_get_level(GPIO_PLUG_SENSE) == 1 || enable_power;
    }

    gpio_config_t asic_enable_conf = {
        .pin_bit_mask = (1ULL << GPIO_ASIC_ENABLE),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&asic_enable_conf);

    bool active_high = GLOBAL_STATE->DEVICE_CONFIG.asic_enable_active_high;
    gpio_set_level(GPIO_ASIC_ENABLE, enable_power ? active_high : !active_high);

    if (enable_power) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t VCORE_init(GlobalState * GLOBAL_STATE)
{
    vcore_initialized = false;
    ESP_RETURN_ON_FALSE(GLOBAL_STATE->DEVICE_CONFIG.family.voltage_domains != 0, ESP_FAIL, TAG, "voltage_domains not defined");

    if (GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge) {
        gpio_config_t enable = {
            .pin_bit_mask = 1ULL << GPIO_ASIC_ENABLE,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&enable), TAG,
                            "TPS546 enable GPIO config failed");
        ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_ENABLE, 0), TAG,
                            "TPS546 enable safe-state failed");
        gpio_config_t pgood = {
            .pin_bit_mask = 1ULL << GPIO_TPS546_PGOOD,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&pgood), TAG,
                            "TPS546 PGOOD GPIO config failed");
        esp_err_t bridge_err = BZM_bridge_init();
        if (bridge_err == ESP_OK) {
            bridge_err = BZM_bridge_set_5v_enabled(false);
        }
        if (bridge_err != ESP_OK) {
            /*
             * Keep initializing the independently controlled TPS546 in its
             * off state. A blank RP2040 must not prevent Wi-Fi/HTTP recovery,
             * and the production controller will refuse to mine without the
             * missing bridge readback.
             */
            ESP_LOGE(TAG,
                     "Bonanza bridge safe-state unavailable: %s; "
                     "keeping the TPS546 off for HTTP bridge recovery",
                     esp_err_to_name(bridge_err));
        }
    }
    else {
        configure_asic_power_enable(GLOBAL_STATE);
    }

    if (GLOBAL_STATE->DEVICE_CONFIG.DS4432U) {
        ESP_RETURN_ON_ERROR(DS4432U_init(), TAG, "DS4432 init failed!");
    }
    if (GLOBAL_STATE->DEVICE_CONFIG.INA260) {
        ESP_RETURN_ON_ERROR(INA260_init(), TAG, "INA260 init failed!");
    }
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        TPS546_CONFIG tps_config = get_tps546_config(&GLOBAL_STATE->DEVICE_CONFIG.family);
        ESP_RETURN_ON_ERROR(TPS546_init(tps_config), TAG, "TPS546 init failed!");
    }

    vcore_initialized = true;
    return ESP_OK;
}

static esp_err_t bonanza_set_5v_enabled(void *context, bool enabled)
{
    (void)context;
    return BZM_bridge_set_5v_enabled(enabled);
}

static esp_err_t bonanza_set_regulator_enabled(void *context, bool enabled)
{
    (void)context;
    return gpio_set_level(GPIO_ASIC_ENABLE, enabled ? 1 : 0);
}

static esp_err_t bonanza_set_vout(void *context, float volts)
{
    GlobalState *state = context;
    if (volts != 0.0f &&
        !bzm_power_runtime_voltage_is_allowed(volts)) {
        ESP_LOGE(TAG,
                 "Bitaxe 1002 TPS rail request %.3fV is outside 2.1..3.2V",
                 volts);
        return ESP_ERR_INVALID_ARG;
    }
    return state->DEVICE_CONFIG.TPS546 ? TPS546_set_vout(volts)
                                       : ESP_ERR_INVALID_STATE;
}

static esp_err_t bonanza_validate_power(void *context, float expected_vout)
{
    (void)context;
    if (!bzm_power_runtime_voltage_is_allowed(expected_vout) ||
        gpio_get_level(GPIO_TPS546_PGOOD) == 0) {
        ESP_LOGE(TAG, "TPS546 PGOOD remained low");
        return ESP_FAIL;
    }

    TPS546_StatusSnapshot snapshot;
    ESP_RETURN_ON_ERROR(TPS546_snapshot_status(&snapshot), TAG,
                        "TPS546 status/telemetry read failed");
    if ((snapshot.status_word &
         (TPS546_STATUS_VOUT | TPS546_STATUS_IOUT |
          TPS546_STATUS_INPUT | TPS546_STATUS_PGOOD |
          TPS546_STATUS_OFF | TPS546_STATUS_TEMP |
          TPS546_STATUS_CML)) != 0 ||
        (snapshot.operation & 0x80) == 0 ||
        !isfinite(snapshot.vout_command) ||
        fabsf(snapshot.vout_command - expected_vout) >
            BZM_TPS546_VOUT_READBACK_TOLERANCE_V ||
        snapshot.read_vin < BZM_TPS546_BIRDS_PROFILE.vin_off ||
        snapshot.read_vout <
            expected_vout - BZM_TPS546_VOUT_OPERATING_TOLERANCE_V ||
        snapshot.read_vout >
            expected_vout + BZM_TPS546_VOUT_OPERATING_TOLERANCE_V ||
        snapshot.read_temp1 >= BZM_TPS546_BIRDS_PROFILE.ot_fault_limit) {
        TPS546_log_snapshot(&snapshot);
        ESP_LOGE(TAG,
                 "TPS546 %.3fV command/status/telemetry validation failed",
                 expected_vout);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void bonanza_power_delay(void *context, uint32_t delay_ms)
{
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static const bzm_power_ops_t BONANZA_POWER_OPS = {
    .set_5v_enabled = bonanza_set_5v_enabled,
    .set_regulator_enabled = bonanza_set_regulator_enabled,
    .set_vout = bonanza_set_vout,
    .validate_power = bonanza_validate_power,
    .delay_ms = bonanza_power_delay,
};

esp_err_t VCORE_bzm_set_rail_enabled(GlobalState *GLOBAL_STATE, bool enabled)
{
    if (GLOBAL_STATE == NULL ||
        !GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge ||
        !GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return ESP_ERR_INVALID_STATE;
    }
    return bzm_power_set_rail_enabled(
        &BONANZA_POWER_OPS, GLOBAL_STATE, enabled);
}

esp_err_t VCORE_bzm_set_runtime_voltage(GlobalState *GLOBAL_STATE,
                                        float volts)
{
    if (GLOBAL_STATE == NULL ||
        !GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge ||
        !GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return ESP_ERR_INVALID_STATE;
    }
    return bzm_power_set_runtime_voltage(
        &BONANZA_POWER_OPS, GLOBAL_STATE, volts);
}

esp_err_t VCORE_bzm_force_regulator_off(GlobalState *GLOBAL_STATE)
{
    if (GLOBAL_STATE == NULL ||
        !GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge ||
        !GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * This recovery-only primitive deliberately does not contact the RP2040.
     * Drop the ESP-owned hardware enable first, then command the TPS546 off.
     */
    esp_err_t gpio_err = gpio_set_level(GPIO_ASIC_ENABLE, 0);
    esp_err_t tps_err = TPS546_set_vout(0.0f);
    return gpio_err != ESP_OK ? gpio_err : tps_err;
}

esp_err_t VCORE_bzm_snapshot(TPS546_StatusSnapshot *snapshot, bool *pgood)
{
    if (snapshot == NULL || pgood == NULL) return ESP_ERR_INVALID_ARG;
    *pgood = gpio_get_level(GPIO_TPS546_PGOOD) != 0;
    return TPS546_snapshot_status(snapshot);
}

bool VCORE_is_initialized(void)
{
    return vcore_initialized;
}

esp_err_t VCORE_set_voltage(GlobalState * GLOBAL_STATE, float core_voltage)
{
    ESP_LOGI(TAG, "Set ASIC voltage = %.3fV", core_voltage);

    if (GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge) {
        if (!bzm_power_voltage_is_allowed(core_voltage)) {
            ESP_LOGE(TAG,
                     "Bitaxe 1002 startup rail accepts only off or 2.800V");
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t err = bzm_power_set_enabled(
            &BONANZA_POWER_OPS, GLOBAL_STATE, core_voltage != 0.0f);
        if (err != ESP_OK && core_voltage != 0.0f) {
            GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg,
                     sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                     "Bonanza power-up validation failed");
        }
        return err;
    }

    // Enable/disable the ASIC power enable GPIO before touching the regulator
    if (GLOBAL_STATE->DEVICE_CONFIG.asic_enable) {
        bool active_high = GLOBAL_STATE->DEVICE_CONFIG.asic_enable_active_high;
        bool enable_power = core_voltage != 0.0f;
        gpio_set_level(GPIO_ASIC_ENABLE, enable_power ? active_high : !active_high);

        if (enable_power) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    if (GLOBAL_STATE->DEVICE_CONFIG.DS4432U) {
        if (core_voltage != 0.0f) {
            ESP_RETURN_ON_ERROR(DS4432U_set_voltage(core_voltage), TAG, "DS4432U set voltage failed!");
        }
    }
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        uint16_t voltage_domains = GLOBAL_STATE->DEVICE_CONFIG.family.voltage_domains;
        ESP_RETURN_ON_ERROR(TPS546_set_vout(core_voltage * voltage_domains), TAG, "TPS546 set voltage failed!");
    }

    return ESP_OK;
}

int16_t VCORE_get_voltage_mv(GlobalState * GLOBAL_STATE)
{
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return TPS546_get_vout() / GLOBAL_STATE->DEVICE_CONFIG.family.voltage_domains * 1000;
    }
    return ADC_get_vcore();
}

// Lowest core voltage (mV) the regulator will accept. TPS546_set_vout() rejects
// anything below VOUT_MIN as out of range, so callers must not command below this.
int16_t VCORE_get_voltage_min_mv(GlobalState * GLOBAL_STATE)
{
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        TPS546_CONFIG config = get_tps546_config(&GLOBAL_STATE->DEVICE_CONFIG.family);
        uint16_t domains = GLOBAL_STATE->DEVICE_CONFIG.family.voltage_domains;
        if (domains == 0) domains = 1;
        // Round up so core_mv * domains never lands just below VOUT_MIN.
        return (int16_t) ceilf(config.TPS546_INIT_VOUT_MIN / domains * 1000.0f);
    }
    return 0; // non-TPS546 boards have no PMBus minimum
}

esp_err_t VCORE_check_fault(GlobalState * GLOBAL_STATE)
{
    if (GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge) {
        bool tripped = false;
        esp_err_t err = BZM_bridge_get_asic_trip(&tripped);
        if (err != ESP_OK) {
            GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg,
                     sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                     "Bonanza control bridge unavailable");
            bzm_power_set_enabled(&BONANZA_POWER_OPS, GLOBAL_STATE, false);
            ESP_RETURN_ON_ERROR(err, TAG, "Bonanza ASIC trip read failed");
        }
        if (tripped) {
            GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg,
                     sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                     "Bonanza ASIC trip asserted");
            bzm_power_set_enabled(&BONANZA_POWER_OPS, GLOBAL_STATE, false);
            return ESP_FAIL;
        }
    }
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        esp_err_t err = TPS546_check_status(GLOBAL_STATE);
        if (err != ESP_OK) {
            if (GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge) {
                GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
                snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg,
                         sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                         "Bonanza TPS546 telemetry unavailable");
                bzm_power_set_enabled(&BONANZA_POWER_OPS, GLOBAL_STATE,
                                      false);
            }
            ESP_RETURN_ON_ERROR(err, TAG, "TPS546 check status failed!");
        }
        if (GLOBAL_STATE->DEVICE_CONFIG.bonanza_bridge &&
            GLOBAL_STATE->SYSTEM_MODULE.power_fault) {
            GLOBAL_STATE->SYSTEM_MODULE.hardware_fault = true;
            snprintf(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg,
                     sizeof(GLOBAL_STATE->SYSTEM_MODULE.hardware_fault_msg),
                     "Bonanza TPS546 fault asserted");
            bzm_power_set_enabled(&BONANZA_POWER_OPS, GLOBAL_STATE, false);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

const char * VCORE_get_fault_string(GlobalState * GLOBAL_STATE)
{
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return TPS546_get_error_message();
    }
    return NULL;
}

uint8_t VCORE_get_phase_count(GlobalState * GLOBAL_STATE)
{
    if (GLOBAL_STATE->DEVICE_CONFIG.TPS546) {
        return TPS546_get_phase_count();
    }
    return 1;
}
