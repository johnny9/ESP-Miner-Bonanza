#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"

#include "DS4432U.h"
#include "thermal.h"
#include "vcore.h"
#include "power.h"
#include "TPS546.h"
#include "nvs_config.h"
#include "global_state.h"
#include "asic.h"
#include "asic_reset.h"
#include "device_config.h"
#include "PID.h"
#include "self_test.h"
#include "self_test_policy.h"
#include "bzm_controller.h"
#include "utils.h"

#define GPIO_ASIC_ENABLE CONFIG_GPIO_ASIC_ENABLE

/////Test Constants/////
// Test Fan Speed
#define SELF_TEST_MIN_FAN_PERCENT 10.0f
#define SELF_TEST_MAX_FAN_PERCENT 100.0f
#define SELF_TEST_PID_SAMPLE_TIME_MS 100
#define SELF_TEST_PID_P 5.0f
#define SELF_TEST_PID_I 0.1f
#define SELF_TEST_PID_D 2.0f
#define SELF_TEST_POWER_MONITOR_STOP_MS 150
#define SELF_TEST_DOMAIN_HASHRATE_TOLERANCE 0.33f
#define SELF_TEST_DOMAIN_REJECTED_WARN_RATIO 0.25f

#define SELF_TEST_CORE_VOLTAGE_TOLERANCE 0.10f

// Test Power Consumption
#define DEFAULT_POWER_CONSUMPTION_MARGIN 3 // watts above target
#define MULTIPHASE_BUCK_MIN_CURRENT_A 1.0f

// Test Input Voltage
#define INPUT_VOLTAGE_MARGIN 0.10f // +/- 10%

#define MULTIPHASE_BUCK_MIN_CURRENT_A 1.0f

// Test Difficulty
#define DIFFICULTY 16

static const char * TAG = "self_test";

static SemaphoreHandle_t longPressSemaphore;
static bool isFactoryTest = false;
static GlobalState *self_test_state;

// local function prototypes
static void tests_done(GlobalState * GLOBAL_STATE, bool test_result);

typedef self_test_domain_average_t SelfTestDomainAverage;

typedef struct {
    int asic_count;
    int hash_domains;
    SelfTestDomainAverage *domains;
} SelfTestDomainAverages;

static size_t self_test_domain_index(const SelfTestDomainAverages * averages, int asic_nr, int domain_nr)
{
    return (size_t)asic_nr * averages->hash_domains + domain_nr;
}

static SelfTestDomainAverage * self_test_domain_get(SelfTestDomainAverages * averages, int asic_nr, int domain_nr)
{
    return &averages->domains[self_test_domain_index(averages, asic_nr, domain_nr)];
}

static esp_err_t self_test_domain_averages_init(SelfTestDomainAverages * averages, int asic_count, int hash_domains)
{
    memset(averages, 0, sizeof(*averages));
    averages->asic_count = asic_count;
    averages->hash_domains = hash_domains;

    size_t domain_count = (size_t)asic_count * hash_domains;
    averages->domains = calloc(domain_count, sizeof(*averages->domains));
    if (!averages->domains) {
        memset(averages, 0, sizeof(*averages));
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void self_test_domain_averages_free(SelfTestDomainAverages * averages)
{
    free(averages->domains);
    memset(averages, 0, sizeof(*averages));
}

static void self_test_domain_averages_prime(GlobalState * GLOBAL_STATE, SelfTestDomainAverages * averages)
{
    if (!averages->domains) {
        return;
    }

    for (int asic_nr = 0; asic_nr < averages->asic_count; asic_nr++) {
        for (int domain_nr = 0; domain_nr < averages->hash_domains; domain_nr++) {
            asic_domain_measurement_t measurement;
            if (ASIC_get_domain_measurement(GLOBAL_STATE, asic_nr, domain_nr, &measurement) != ESP_OK) {
                continue;
            }
            SelfTestDomainAverage * average = self_test_domain_get(averages, asic_nr, domain_nr);
            average->last_sample_time_us = measurement.time_us;
        }
    }
}

static void self_test_domain_averages_sample(GlobalState * GLOBAL_STATE,
                                             SelfTestDomainAverages * averages,
                                             float expected_domain_hashrate)
{
    if (!averages->domains) {
        return;
    }

    for (int asic_nr = 0; asic_nr < averages->asic_count; asic_nr++) {
        for (int domain_nr = 0; domain_nr < averages->hash_domains; domain_nr++) {
            asic_domain_measurement_t measurement;
            if (ASIC_get_domain_measurement(GLOBAL_STATE, asic_nr, domain_nr, &measurement) != ESP_OK)
                continue;
            self_test_domain_add(self_test_domain_get(averages, asic_nr, domain_nr),
                (self_test_domain_sample_t){
                    .hashrate_ghs = measurement.hashrate,
                    .acquired_at_us = measurement.time_us,
                    .generation = GLOBAL_STATE->SELF_TEST_MODULE.work_generation,
                    .valid = GLOBAL_STATE->ASIC_initalized,
                }, expected_domain_hashrate);
        }
    }
}

static const SelfTestDomainAverage * self_test_domain_get_const(const SelfTestDomainAverages * averages, int asic_nr, int domain_nr)
{
    return &averages->domains[self_test_domain_index(averages, asic_nr, domain_nr)];
}

static float self_test_domain_average_hashrate(const SelfTestDomainAverage * average)
{
    return average->sample_count > 0 ? average->hashrate_sum / average->sample_count : 0.0f;
}

static void self_test_set_fan_percent(GlobalState * GLOBAL_STATE, float fan_percent)
{
    if (fan_percent > SELF_TEST_MAX_FAN_PERCENT) fan_percent = SELF_TEST_MAX_FAN_PERCENT;
    float minimum = fmaxf(SELF_TEST_MIN_FAN_PERCENT,
                           Thermal_get_fan_min_percent(&GLOBAL_STATE->DEVICE_CONFIG));
    if (fan_percent < minimum) fan_percent = minimum;

    GLOBAL_STATE->POWER_MANAGEMENT_MODULE.fan_perc = fan_percent;
    if (Thermal_set_fan_percent(&GLOBAL_STATE->DEVICE_CONFIG, fan_percent / 100.0f) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set fan speed to %.1f%%", fan_percent);
        self_test_show_message(GLOBAL_STATE, "FAN:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
}

static bool self_test_temp_invalid(float temp)
{
    return !isfinite(temp) || temp <= 0.0f || temp >= 127.0f;
}

static float self_test_get_control_temp(GlobalState * GLOBAL_STATE)
{
    float temp = Thermal_get_chip_temp(GLOBAL_STATE);
    float temp2 = Thermal_get_chip_temp2(GLOBAL_STATE);

    if (self_test_temp_invalid(temp)) {
        return temp2;
    }

    if (!self_test_temp_invalid(temp2) && temp2 > temp) {
        return temp2;
    }

    return temp;
}

static float self_test_get_valid_control_temp(GlobalState * GLOBAL_STATE)
{
    float temp = self_test_get_control_temp(GLOBAL_STATE);
    if (self_test_temp_invalid(temp)) {
        ESP_LOGE(TAG, "Open circuit or no result on temperature sensor: %.1f°C", temp);
        self_test_show_message(GLOBAL_STATE, "TEMP:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
    return temp;
}

static void self_test_start_nonce_measurement(GlobalState * GLOBAL_STATE)
{
    SelfTestNonceMeasurement * measurement = &GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement;

    pthread_mutex_lock(&measurement->lock);
    measurement->accepted_count = 0;
    measurement->rejected_count = 0;
    measurement->hashes = 0.0;
    measurement->is_active = true;
    pthread_mutex_unlock(&measurement->lock);
}

static void self_test_stop_nonce_measurement(GlobalState * GLOBAL_STATE)
{
    SelfTestNonceMeasurement * measurement = &GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement;

    pthread_mutex_lock(&measurement->lock);
    measurement->is_active = false;
    pthread_mutex_unlock(&measurement->lock);
}

static void self_test_get_nonce_measurement(GlobalState * GLOBAL_STATE,
                                            uint64_t * accepted_count,
                                            uint64_t * rejected_count,
                                            double * hashes)
{
    SelfTestNonceMeasurement * measurement = &GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement;

    pthread_mutex_lock(&measurement->lock);
    if (accepted_count) *accepted_count = measurement->accepted_count;
    if (rejected_count) *rejected_count = measurement->rejected_count;
    if (hashes) *hashes = measurement->hashes;
    pthread_mutex_unlock(&measurement->lock);
}

static float self_test_get_nonce_hashrate(GlobalState * GLOBAL_STATE, uint64_t elapsed_us)
{
    if (elapsed_us == 0) {
        return 0.0f;
    }

    double hashes;
    self_test_get_nonce_measurement(GLOBAL_STATE, NULL, NULL, &hashes);

    double seconds = elapsed_us / 1000000.0;
    return (float)(hashes / seconds / 1000000000.0);
}

void self_test_record_nonce(GlobalState * GLOBAL_STATE, double nonce_diff)
{
    SelfTestNonceMeasurement * measurement = &GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement;
    double ticket_diff = GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty;

    pthread_mutex_lock(&measurement->lock);
    if (measurement->is_active) {
        if (nonce_diff >= ticket_diff) {
            measurement->accepted_count++;
            measurement->hashes += ticket_diff * NONCE_SPACE;
        } else {
            measurement->rejected_count++;
        }
    }
    pthread_mutex_unlock(&measurement->lock);
}

static void self_test_check_running(GlobalState *state)
{
    if (atomic_load(&state->SELF_TEST_MODULE.cancel_requested)) {
        self_test_show_message(state, "CANCELLED");
        tests_done(state, false);
    }
    if (atomic_load(&state->SELF_TEST_MODULE.worker_failed) ||
        !state->ASIC_initalized || state->SYSTEM_MODULE.hardware_fault ||
        state->SYSTEM_MODULE.power_fault) {
        self_test_show_message(state, "WORK:FAIL");
        tests_done(state, false);
    }
}

static bool self_test_should_run()
{
    if (nvs_config_get_bool(NVS_CONFIG_SELF_TEST_MANUAL)) {
        nvs_config_set_bool(NVS_CONFIG_SELF_TEST_MANUAL, false);
        return true;
    }
    bool is_factory_flash = nvs_config_get_u64(NVS_CONFIG_BEST_DIFF) < 1;
    bool is_self_test_flag_set = nvs_config_get_bool(NVS_CONFIG_SELF_TEST);
    if (is_factory_flash && is_self_test_flag_set) {
        isFactoryTest = true;
        return true;
    }

    // Optionally start self-test when boot button is pressed
    return gpio_get_level(CONFIG_GPIO_BUTTON_BOOT) == 0; // LOW when pressed
}

esp_err_t self_test_init(GlobalState * GLOBAL_STATE)
{
    self_test_state = GLOBAL_STATE;
    if (pthread_mutex_init(&GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement.lock, NULL) != 0)
        return ESP_ERR_NO_MEM;
    if (self_test_should_run()) {
        GLOBAL_STATE->SELF_TEST_MODULE.is_active = true;
        atomic_store(&GLOBAL_STATE->SELF_TEST_MODULE.status, SELF_TEST_RUNNING);
        GLOBAL_STATE->SELF_TEST_MODULE.is_factory = isFactoryTest;
        GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty = DIFFICULTY;

        // Create a binary semaphore
        longPressSemaphore = xSemaphoreCreateBinary();

        if (longPressSemaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create semaphore");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

void self_test_reset()
{
    if (self_test_state != NULL) {
        atomic_store(&self_test_state->SELF_TEST_MODULE.cancel_requested, true);
        self_test_stop_work(self_test_state);
    }
    if (longPressSemaphore != NULL) {
        ESP_LOGI(TAG, "Long press detected...");
        // Give the semaphore back
        xSemaphoreGive(longPressSemaphore);
    }
}

void self_test_show_message(GlobalState * GLOBAL_STATE, const char * msg)
{
    if (!GLOBAL_STATE->SELF_TEST_MODULE.is_active) return;

    pthread_mutex_lock(&GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement.lock);
    snprintf(GLOBAL_STATE->SELF_TEST_MODULE.message_buffer,
             sizeof(GLOBAL_STATE->SELF_TEST_MODULE.message_buffer), "%s", msg);
    GLOBAL_STATE->SELF_TEST_MODULE.message = GLOBAL_STATE->SELF_TEST_MODULE.message_buffer;
    pthread_mutex_unlock(&GLOBAL_STATE->SELF_TEST_MODULE.nonce_measurement.lock);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

static esp_err_t test_fan_sense(GlobalState * GLOBAL_STATE)
{
    uint16_t fan_speed = Thermal_get_fan_speed(&GLOBAL_STATE->DEVICE_CONFIG);
    uint16_t target_speed = GLOBAL_STATE->DEVICE_CONFIG.self_test_fan_target_rpm != 0
                                ? GLOBAL_STATE->DEVICE_CONFIG.self_test_fan_target_rpm
                                : nvs_config_get_u16(NVS_CONFIG_SELF_TEST_FAN_SPEED);

    ESP_LOGI(TAG, "fanSpeed: %d RPM", fan_speed);
    if (fan_speed > target_speed) {
        return ESP_OK;
    }

    // fan test failed
    ESP_LOGE(TAG, "FAN test failed!");
    self_test_show_message(GLOBAL_STATE, "FAN:WARN");
    return ESP_FAIL;
}

static esp_err_t test_power_consumption(GlobalState * GLOBAL_STATE)
{
    float target_power = (float) GLOBAL_STATE->DEVICE_CONFIG.power_consumption_target;
    float margin = (float) GLOBAL_STATE->DEVICE_CONFIG.power_consumption_margin;
    if (margin <= 0.0f) {
        margin = DEFAULT_POWER_CONSUMPTION_MARGIN;
    }
    float maximum_power = target_power > 0 ? target_power + margin :
                          GLOBAL_STATE->DEVICE_CONFIG.family.max_power;

    float power = 0;
    float current = 0;

    uint8_t phase_count = VCORE_get_phase_count(GLOBAL_STATE);
    if (phase_count > 1 &&
        TPS546_check_phase_currents(phase_count, MULTIPHASE_BUCK_MIN_CURRENT_A) != ESP_OK) {
        ESP_LOGE(TAG, "MULTIPHASE BUCK test failed!");
        self_test_show_message(GLOBAL_STATE, "BUCK:FAIL");
        return ESP_FAIL;
    }

    Power_get_output(GLOBAL_STATE, &power, &current);
    ESP_LOGI(TAG, "Power: %.2f W (target: %.2f W, maximum: %.2f W)",
             power, target_power, maximum_power);

    if (isfinite(power) && power > 0 && maximum_power > 0 && power <= maximum_power) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "POWER test failed! measured %.2f W, maximum %.2f W",
             power, maximum_power);
    self_test_show_message(GLOBAL_STATE, "POWER:FAIL");
    return ESP_FAIL;
}

static esp_err_t test_core_voltage(GlobalState * GLOBAL_STATE)
{
    uint16_t core_voltage = VCORE_get_voltage_mv(GLOBAL_STATE);
    uint16_t target_voltage = GLOBAL_STATE->DEVICE_CONFIG.family.asic.default_voltage_mv;
    float margin = target_voltage * SELF_TEST_CORE_VOLTAGE_TOLERANCE;
    ESP_LOGI(TAG, "Core voltage: %u mV (target: %u mV +/- %.0f mV)", core_voltage, target_voltage, margin);

    if (core_voltage >= target_voltage - margin && core_voltage <= target_voltage + margin) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Core voltage test failed");
    self_test_show_message(GLOBAL_STATE, "VCORE:FAIL");
    return ESP_FAIL;
}

static esp_err_t test_input_voltage(GlobalState * GLOBAL_STATE)
{
    if (!GLOBAL_STATE->DEVICE_CONFIG.INA260) {
        return ESP_OK;
    }

    float input_voltage_mv = Power_get_input_voltage(GLOBAL_STATE);
    float nominal_mv = GLOBAL_STATE->DEVICE_CONFIG.family.nominal_voltage * 1000.0f;
    float margin_mv = nominal_mv * INPUT_VOLTAGE_MARGIN;

    ESP_LOGI(TAG, "Input voltage: %.0f mV (nominal: %.0f mV +/- %.0f mV)", input_voltage_mv, nominal_mv, margin_mv);

    if (input_voltage_mv >= nominal_mv - margin_mv && input_voltage_mv <= nominal_mv + margin_mv) {
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Input voltage test failed! %.0f mV, expected %.0f +/- %.0f mV", input_voltage_mv, nominal_mv, margin_mv);
    self_test_show_message(GLOBAL_STATE, "VIN:FAIL");
    return ESP_FAIL;
}

esp_err_t test_vreg_faults(GlobalState * GLOBAL_STATE)
{
    // check for faults on the voltage regulator
    ESP_RETURN_ON_ERROR(VCORE_check_fault(GLOBAL_STATE), TAG, "VCORE check fault failed!");

    if (GLOBAL_STATE->SYSTEM_MODULE.power_fault) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Perform a self-test of the system.
 *
 * This function is run as a task and will execute a series of
 * diagnostic tests to ensure the system is functioning correctly.
 *
 * @param pvParameters Pointer to the parameters passed to the task (if any).
 */
void self_test_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    if (!GLOBAL_STATE->SELF_TEST_MODULE.is_active) return;

    // Check if we already have an error message from peripheral initialization
    if (GLOBAL_STATE->SELF_TEST_MODULE.system_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Aborting self-test due to initialization failure: %s", GLOBAL_STATE->SELF_TEST_MODULE.message);
        tests_done(GLOBAL_STATE, false);
    }

    if (isFactoryTest) {
        ESP_LOGI(TAG, "Running factory self-test");
    } else {
        ESP_LOGI(TAG, "Running manual self-test");
    }

    char logString[300];

    if (!GLOBAL_STATE->psram_is_available) {
        ESP_LOGE(TAG, "NO PSRAM on device!");
        self_test_show_message(GLOBAL_STATE, "PSRAM:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    // Capture extra validation for DS4432U if present
    if (GLOBAL_STATE->DEVICE_CONFIG.DS4432U && DS4432U_test() != ESP_OK) {
        ESP_LOGE(TAG, "DS4432 test failed!");
        self_test_show_message(GLOBAL_STATE, "DS4432U:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    // Input voltage check (INA260 devices only)
    if (test_input_voltage(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Input voltage test failed!");
        self_test_show_message(GLOBAL_STATE, "VOLTAGE:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    // test for voltage regulator faults
    if (test_vreg_faults(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "VCORE check fault failed!");
        self_test_show_message(GLOBAL_STATE, "VCORE:PWR FAULT");
        tests_done(GLOBAL_STATE, false);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    float target_temp = (float)nvs_config_get_u16(NVS_CONFIG_SELF_TEST_TEMP_TARGET);
    float warmup_temp = (float)nvs_config_get_u16(NVS_CONFIG_SELF_TEST_TEMP_WARMUP);
    float max_temp    = (float)nvs_config_get_u16(NVS_CONFIG_SELF_TEST_TEMP_MAX);

    if (!isfinite(target_temp) || warmup_temp <= 0 || warmup_temp > target_temp ||
        target_temp >= max_temp || max_temp > 90) {
        self_test_show_message(GLOBAL_STATE, "CONFIG:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
    float expected_hashrate = GLOBAL_STATE->POWER_MANAGEMENT_MODULE.expected_hashrate *
                              GLOBAL_STATE->DEVICE_CONFIG.family.asic.hashrate_test_percentage_target;
    if (!isfinite(expected_hashrate) || expected_hashrate <= 0 ||
        GLOBAL_STATE->DEVICE_CONFIG.family.asic.hash_domains == 0 ||
        GLOBAL_STATE->DEVICE_CONFIG.family.asic_count == 0) {
        self_test_show_message(GLOBAL_STATE, "CONFIG:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
    if (!self_test_start_work(GLOBAL_STATE)) {
        self_test_show_message(GLOBAL_STATE, "WORK:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    self_test_set_fan_percent(GLOBAL_STATE, SELF_TEST_MAX_FAN_PERCENT);

    uint64_t readiness_start = esp_timer_get_time();
    while (self_test_temp_invalid(self_test_get_control_temp(GLOBAL_STATE)) ||
           VCORE_get_voltage_mv(GLOBAL_STATE) <= 0 ||
           Thermal_get_fan_speed(&GLOBAL_STATE->DEVICE_CONFIG) == 0) {
        self_test_check_running(GLOBAL_STATE);
        if (self_test_deadline_expired(readiness_start, esp_timer_get_time(), 15000000)) {
            self_test_show_message(GLOBAL_STATE, "READY:TIMEOUT");
            tests_done(GLOBAL_STATE, false);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    float asic_temp = self_test_get_valid_control_temp(GLOBAL_STATE);
    if (asic_temp >= max_temp) {
        self_test_show_message(GLOBAL_STATE, "TEMP:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
    ESP_LOGI(TAG, "ASIC Temp %.1f°C", asic_temp);

    self_test_set_fan_percent(GLOBAL_STATE, SELF_TEST_MIN_FAN_PERCENT);
    uint64_t warmup_start = esp_timer_get_time();
    while (asic_temp < warmup_temp)
    {
        self_test_check_running(GLOBAL_STATE);
        if (self_test_deadline_expired(warmup_start, esp_timer_get_time(), 120000000)) {
            self_test_show_message(GLOBAL_STATE, "WARMUP:TIMEOUT");
            tests_done(GLOBAL_STATE, false);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        asic_temp = self_test_get_valid_control_temp(GLOBAL_STATE);
        if (asic_temp >= max_temp) {
            self_test_show_message(GLOBAL_STATE, "TEMP:FAIL");
            tests_done(GLOBAL_STATE, false);
        }
        ESP_LOGI(TAG, "Warming up to %.1f°C: %.1f°C", warmup_temp, asic_temp);
        snprintf(logString, sizeof(logString), "ASIC Temp: %.1f°C", asic_temp);
        self_test_show_message(GLOBAL_STATE, logString);
    }

    PIDController pid = {0};
    float pid_input = asic_temp;
    float minimum_fan = fmaxf(SELF_TEST_MIN_FAN_PERCENT,
                               Thermal_get_fan_min_percent(&GLOBAL_STATE->DEVICE_CONFIG));
    float pid_output = minimum_fan;
    float pid_setpoint = target_temp;
    pid_init(&pid, &pid_input, &pid_output, &pid_setpoint,
             SELF_TEST_PID_P, SELF_TEST_PID_I, SELF_TEST_PID_D, PID_P_ON_E, PID_REVERSE);
    pid_set_sample_time(&pid, SELF_TEST_PID_SAMPLE_TIME_MS);
    pid_set_output_limits(&pid, minimum_fan, SELF_TEST_MAX_FAN_PERCENT);
    pid_set_mode(&pid, AUTOMATIC);

    uint64_t start_us = esp_timer_get_time();
    uint64_t hashtest_us = 30000000;
    float hashrate = 0;
    float expected_domain_hashrate = expected_hashrate /
                                     GLOBAL_STATE->DEVICE_CONFIG.family.asic.hash_domains /
                                     GLOBAL_STATE->DEVICE_CONFIG.family.asic_count;

    SelfTestDomainAverages domain_averages;
    if (self_test_domain_averages_init(&domain_averages,
                                       GLOBAL_STATE->DEVICE_CONFIG.family.asic_count,
                                       GLOBAL_STATE->DEVICE_CONFIG.family.asic.hash_domains) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate domain hashrate averages");
        self_test_show_message(GLOBAL_STATE, "MEM:FAIL");
        tests_done(GLOBAL_STATE, false);
    }
    GLOBAL_STATE->SELF_TEST_MODULE.domain_averages = domain_averages.domains;
    self_test_domain_averages_prime(GLOBAL_STATE, &domain_averages);

    self_test_start_nonce_measurement(GLOBAL_STATE);
    ESP_LOGI(TAG, "Starting 30s hashrate monitoring loop, target temp %.1f°C", target_temp);
    while ((esp_timer_get_time() - start_us) < hashtest_us) {
        self_test_check_running(GLOBAL_STATE);
        uint64_t elapsed_us = esp_timer_get_time() - start_us;
        hashrate = self_test_get_nonce_hashrate(GLOBAL_STATE, elapsed_us);
        asic_temp = self_test_get_valid_control_temp(GLOBAL_STATE);
        pid_input = asic_temp;
        if (pid_compute(&pid)) {
            self_test_set_fan_percent(GLOBAL_STATE, pid_output);
        }
        
        if (asic_temp > max_temp) {
            ESP_LOGE(TAG, "Overheat: %.1f°C", asic_temp);
            snprintf(logString, sizeof(logString), "TEMP:FAIL: %.1f°C", asic_temp);
            self_test_show_message(GLOBAL_STATE, logString);
            tests_done(GLOBAL_STATE, false);
        }

        uint32_t remaining = elapsed_us < hashtest_us ? (hashtest_us - elapsed_us) / 1000000 : 0;
        snprintf(logString, sizeof(logString), "%.0f Gh/s %.1f°C %" PRIu32 "s", hashrate, asic_temp, remaining);
        ESP_LOGI(TAG, "%s", logString);

        self_test_show_message(GLOBAL_STATE, logString);

        self_test_domain_averages_sample(GLOBAL_STATE, &domain_averages, expected_domain_hashrate);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    self_test_stop_nonce_measurement(GLOBAL_STATE);
    hashrate = self_test_get_nonce_hashrate(GLOBAL_STATE, esp_timer_get_time() - start_us);
    self_test_domain_averages_sample(GLOBAL_STATE, &domain_averages, expected_domain_hashrate);

    uint64_t accepted_count;
    uint64_t rejected_count;
    self_test_get_nonce_measurement(GLOBAL_STATE, &accepted_count, &rejected_count, NULL);
    ESP_LOGI(TAG, "Hashrate: %.2f Gh/s, Expected: %.2f Gh/s", hashrate, expected_hashrate);
    ESP_LOGI(TAG,
             "Nonce measurement: %llu valid, %llu rejected",
             (unsigned long long)accepted_count,
             (unsigned long long)rejected_count);

    // Check domain hashrates from monitor module
    bool domain_failed = false;
    uint32_t failed_asic_mask = 0;
    for (int asic_nr = 0; asic_nr < GLOBAL_STATE->DEVICE_CONFIG.family.asic_count; asic_nr++) {
        int hash_domains = GLOBAL_STATE->DEVICE_CONFIG.family.asic.hash_domains;
        for (int domain_nr = 0; domain_nr < hash_domains; domain_nr++) {
            const SelfTestDomainAverage * domain_average = self_test_domain_get_const(&domain_averages, asic_nr, domain_nr);
            float domain_hashrate = self_test_domain_average_hashrate(domain_average);
            uint32_t sample_count = domain_average->sample_count;
            uint32_t rejected_sample_count = domain_average->rejected_sample_count;
            self_test_domain_status_t domain_status =
                self_test_domain_status(domain_average, expected_domain_hashrate);
            asic_domain_measurement_t current;
            uint64_t now = esp_timer_get_time();
            if (ASIC_get_domain_measurement(GLOBAL_STATE, asic_nr, domain_nr, &current) != ESP_OK ||
                current.time_us == 0 || current.time_us > now ||
                now - current.time_us > 3000000 ||
                current.time_us != domain_average->last_sample_time_us)
                domain_status = SELF_TEST_DOMAIN_FAIL;
            ESP_LOGI(TAG, "ASIC %d domain %d %.2f GH/s: %lu samples, %lu rejected", asic_nr,
                     domain_nr, domain_hashrate, (unsigned long)sample_count,
                     (unsigned long)rejected_sample_count);

            if (domain_status != SELF_TEST_DOMAIN_OK) {
                domain_failed = true;
                if (asic_nr < 32) {
                    failed_asic_mask |= (1u << asic_nr);
                }
            }
        }
    }
    self_test_domain_averages_free(&domain_averages);
    GLOBAL_STATE->SELF_TEST_MODULE.domain_averages = NULL;
    if (domain_failed) {
        if (GLOBAL_STATE->DEVICE_CONFIG.family.asic_count == 2 && failed_asic_mask == 0x3) {
            self_test_show_message(GLOBAL_STATE, "BOTH ASICS DOMAIN:FAIL");
        } else {
            int failed_asic = -1;
            for (int asic_nr = 0; asic_nr < GLOBAL_STATE->DEVICE_CONFIG.family.asic_count && asic_nr < 32; asic_nr++) {
                if (failed_asic_mask & (1u << asic_nr)) {
                    failed_asic = asic_nr;
                    break;
                }
            }

            if (failed_asic >= 0) {
                snprintf(logString, sizeof(logString), "ASIC %d DOMAIN:FAIL", failed_asic);
                self_test_show_message(GLOBAL_STATE, logString);
            } else {
                self_test_show_message(GLOBAL_STATE, "DOMAIN:FAIL");
            }
        }
        tests_done(GLOBAL_STATE, false);
    }

    if (!isfinite(hashrate) || hashrate < expected_hashrate || accepted_count < SELF_TEST_MIN_NONCES) {
        ESP_LOGE(TAG, "Total hashrate too low");
        self_test_show_message(GLOBAL_STATE, "HASHRATE:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    if (test_core_voltage(GLOBAL_STATE) != ESP_OK) {
        tests_done(GLOBAL_STATE, false);
    }

    if (test_power_consumption(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Power Draw Failed, target %.2f W", (float) GLOBAL_STATE->DEVICE_CONFIG.power_consumption_target);
        self_test_show_message(GLOBAL_STATE, "POWER:FAIL");
        tests_done(GLOBAL_STATE, false);
    }

    if (test_fan_sense(GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Fan test failed!");
        tests_done(GLOBAL_STATE, false);
    }

    tests_done(GLOBAL_STATE, true);
}

/**
 * Ends the self test by either resetting or ending the self_test_task
 */
static bool self_test_cleanup(GlobalState *state)
{
    self_test_stop_work(state);
    state->SELF_TEST_MODULE.is_finished = true;
    self_test_stop_nonce_measurement(state);
    bool safe;
    if (state->DEVICE_CONFIG.bonanza_bridge) {
        /* The production controller owns the bridge, regulator and reset.
         * Its verified OFF_SAFE result is the only successful shutdown. */
        safe = bzm_controller_active() && bzm_controller_pause();
    } else {
        safe = asic_hold_reset_low(state) == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(SELF_TEST_POWER_MONITOR_STOP_MS));
        if (VCORE_is_initialized())
            safe = VCORE_set_voltage(state, 0.0f) == ESP_OK && safe;
        state->ASIC_initalized = false;
    }
    uint64_t started = esp_timer_get_time();
    while (atomic_load(&state->SELF_TEST_MODULE.worker_running) &&
           !self_test_deadline_expired(started, esp_timer_get_time(), 15000000))
        vTaskDelay(pdMS_TO_TICKS(10));
    if (atomic_load(&state->SELF_TEST_MODULE.worker_running)) safe = false;
    if (state->asic_job_store.entries != NULL)
        asic_job_store_invalidate_all(&state->asic_job_store);
    if (!atomic_load(&state->SELF_TEST_MODULE.worker_running)) {
        free(state->SELF_TEST_MODULE.domain_averages);
        state->SELF_TEST_MODULE.domain_averages = NULL;
    }
    atomic_store(&state->SELF_TEST_MODULE.cleanup_confirmed, safe);
    if (!safe) {
        (void)Thermal_set_fan_percent(&state->DEVICE_CONFIG, 1.0f);
        self_test_show_message(state, "SHUTDOWN:FAIL");
    }
    return safe;
}

static void tests_done(GlobalState * GLOBAL_STATE, bool isTestPassed)
{
    bool safe = self_test_cleanup(GLOBAL_STATE);
    isTestPassed = isTestPassed && safe &&
        !atomic_load(&GLOBAL_STATE->SELF_TEST_MODULE.cancel_requested);
    GLOBAL_STATE->SELF_TEST_MODULE.result = isTestPassed ? "SELF-TEST PASS!" : "SELF-TEST FAIL!";
    atomic_store(&GLOBAL_STATE->SELF_TEST_MODULE.status, isTestPassed ? SELF_TEST_PASSED :
                 atomic_load(&GLOBAL_STATE->SELF_TEST_MODULE.cancel_requested) ?
                 SELF_TEST_CANCELLED : SELF_TEST_FAILED);
    if (isTestPassed) {
        if (isFactoryTest) nvs_config_set_bool(NVS_CONFIG_SELF_TEST, false);
        GLOBAL_STATE->SELF_TEST_MODULE.finished = "Restarting in 10 seconds";
        ESP_LOGI(TAG, "SELF-TEST PASS! -- Restarting in 10 seconds.");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    }
    GLOBAL_STATE->SELF_TEST_MODULE.finished =
        "Hold BOOT to cancel or retry cleanup, or restart to rerun.";
    ESP_LOGE(TAG, "Self-test failed; cleanup %s", safe ? "confirmed" : "not confirmed");
    for (;;) {
        /* Cancellation never records a pass or clears the factory flag until
         * the worker has exited and power shutdown is confirmed. */
        if (atomic_load(&GLOBAL_STATE->SELF_TEST_MODULE.cancel_requested) && safe) {
            nvs_config_set_bool(NVS_CONFIG_SELF_TEST, false);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
        if (xSemaphoreTake(longPressSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
            safe = self_test_cleanup(GLOBAL_STATE);
    }
}
