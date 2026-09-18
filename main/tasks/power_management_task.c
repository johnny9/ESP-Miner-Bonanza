#include "power_management_task.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include "board_power.h"
#include "bzm_bridge_update.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "global_state.h"
#include "nvs_config.h"
#include "sv1_client.h"

#define POLL_RATE_MS 100U
#define REQUEST_TIMEOUT_MS 180000U

static const char *TAG = "power_management";
static QueueHandle_t requests;
static SemaphoreHandle_t board_io_lock;
static power_operations_t board_operations;
static TaskHandle_t owner_task;
static power_policy_t policy;
static atomic_bool ready;
static atomic_bool initialized;
static atomic_bool fan_allowed;
static atomic_bool cooling;
static atomic_bool maintenance;
static atomic_bool boot_complete;
static atomic_bool boot_success;
/* Stop intent is monotonic, so a concurrent request cannot be erased by a
 * start finishing or a caller timing out. Only an explicit resume accepts it. */
static atomic_uint stop_epoch;
static atomic_uint accepted_epoch;

typedef enum { REQUEST_WAITING, REQUEST_COMPLETE, REQUEST_CANCELLED } request_state_t;
typedef struct {
    power_request_kind_t kind;
    power_owner_t owner;
    unsigned epoch;
    SemaphoreHandle_t completion;
    atomic_uint references;
    atomic_int state;
    bool success;
} power_request_t;

static void release_request(power_request_t *request)
{
    if (atomic_fetch_sub_explicit(&request->references, 1, memory_order_acq_rel) == 1) {
        vSemaphoreDelete(request->completion);
        free(request);
    }
}

bool POWER_MANAGEMENT_stop_requested(void)
{
    return atomic_load_explicit(&stop_epoch, memory_order_acquire) !=
           atomic_load_explicit(&accepted_epoch, memory_order_acquire);
}

static void revoke_work(void)
{
    atomic_fetch_add_explicit(&stop_epoch, 1, memory_order_acq_rel);
}

bool POWER_MANAGEMENT_request(power_request_kind_t kind, power_owner_t owner, uint32_t timeout_ms)
{
    if (kind < POWER_REQUEST_PAUSE || kind > POWER_REQUEST_RELEASE ||
        requests == NULL || !atomic_load_explicit(&initialized, memory_order_acquire)) return false;
    if (kind == POWER_REQUEST_PAUSE || kind == POWER_REQUEST_ACQUIRE) revoke_work();
    power_request_t *request = calloc(1, sizeof(*request));
    if (request == NULL) return false;
    request->completion = xSemaphoreCreateBinary();
    if (request->completion == NULL) { free(request); return false; }
    request->kind = kind;
    request->owner = owner;
    request->epoch = atomic_load_explicit(&stop_epoch, memory_order_acquire);
    atomic_init(&request->references, 2);
    atomic_init(&request->state, REQUEST_WAITING);
    if (xQueueSend(requests, &request, 0) != pdTRUE) {
        release_request(request);
        release_request(request);
        return false;
    }
    if (owner_task != NULL) xTaskNotifyGive(owner_task);
    bool completed = xSemaphoreTake(request->completion, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    if (!completed) {
        int expected = REQUEST_WAITING;
        if (atomic_compare_exchange_strong_explicit(&request->state, &expected,
                REQUEST_CANCELLED, memory_order_acq_rel, memory_order_acquire)) {
            revoke_work();
        } else {
            /* Completion won the timeout race; its result was published
             * before the state transition and is still owned by this caller. */
            completed = expected == REQUEST_COMPLETE;
        }
    }
    bool success = completed && request->success;
    release_request(request);
    return success;
}

bool POWER_MANAGEMENT_pause(void) { return POWER_MANAGEMENT_request(POWER_REQUEST_PAUSE, POWER_OWNER_NONE, REQUEST_TIMEOUT_MS); }
bool POWER_MANAGEMENT_resume(void) { return POWER_MANAGEMENT_request(POWER_REQUEST_RESUME, POWER_OWNER_NONE, REQUEST_TIMEOUT_MS); }
bool POWER_MANAGEMENT_acquire_maintenance(power_owner_t owner) { return POWER_MANAGEMENT_request(POWER_REQUEST_ACQUIRE, owner, REQUEST_TIMEOUT_MS); }
bool POWER_MANAGEMENT_release_maintenance(power_owner_t owner) { return POWER_MANAGEMENT_request(POWER_REQUEST_RELEASE, owner, REQUEST_TIMEOUT_MS); }
bool POWER_MANAGEMENT_prepare_restart(void) { return POWER_MANAGEMENT_request(POWER_REQUEST_ACQUIRE, POWER_OWNER_RESTART, REQUEST_TIMEOUT_MS); }
bool POWER_MANAGEMENT_fan_control_allowed(void)
{
    return atomic_load_explicit(&fan_allowed, memory_order_acquire) && !POWER_MANAGEMENT_stop_requested();
}
bool POWER_MANAGEMENT_board_io_begin(void)
{
    if (!atomic_load_explicit(&initialized, memory_order_acquire)) return false;
    if (board_io_lock == NULL || xSemaphoreTake(board_io_lock, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    if (POWER_MANAGEMENT_in_maintenance()) {
        xSemaphoreGive(board_io_lock);
        return false;
    }
    return true;
}
void POWER_MANAGEMENT_board_io_end(void)
{
    xSemaphoreGive(board_io_lock);
}

static power_start_result_t board_start(void *context)
{
    atomic_store_explicit(&fan_allowed, false, memory_order_release);
    xSemaphoreTake(board_io_lock, portMAX_DELAY);
    power_start_result_t result = board_operations.start(context);
    xSemaphoreGive(board_io_lock);
    return result;
}
static bool board_stop(void *context)
{
    atomic_store_explicit(&fan_allowed, false, memory_order_release);
    xSemaphoreTake(board_io_lock, portMAX_DELAY);
    bool result = board_operations.stop(context);
    xSemaphoreGive(board_io_lock);
    return result;
}
static bool board_maintenance(void *context, power_owner_t owner, bool acquire)
{
    atomic_store_explicit(&fan_allowed, false, memory_order_release);
    xSemaphoreTake(board_io_lock, portMAX_DELAY);
    bool result = board_operations.maintenance(context, owner, acquire);
    xSemaphoreGive(board_io_lock);
    return result;
}

bool POWER_MANAGEMENT_in_maintenance(void)
{
    return atomic_load_explicit(&maintenance, memory_order_acquire);
}
bool POWER_MANAGEMENT_overheat_recovery_active(void)
{
    return atomic_load_explicit(&cooling, memory_order_acquire);
}
void POWER_MANAGEMENT_settings_changed(void)
{
    if (owner_task != NULL) xTaskNotifyGive(owner_task);
}
void POWER_MANAGEMENT_overheat_mode_changed(bool enabled)
{
    (void)enabled;
    POWER_MANAGEMENT_settings_changed();
}

static bool bridge_acquire(void *context)
{
    (void)context;
    return POWER_MANAGEMENT_acquire_maintenance(POWER_OWNER_BRIDGE);
}
static bool bridge_release(void *context)
{
    (void)context;
    return POWER_MANAGEMENT_release_maintenance(POWER_OWNER_BRIDGE);
}
static bool restart_guard(void *context)
{
    (void)context;
    return POWER_MANAGEMENT_prepare_restart();
}

esp_err_t POWER_MANAGEMENT_init(GlobalState *state)
{
    if (state == NULL || requests != NULL) return ESP_ERR_INVALID_STATE;
    requests = xQueueCreate(4, sizeof(power_request_t *));
    if (requests == NULL) return ESP_ERR_NO_MEM;
    board_io_lock = xSemaphoreCreateMutex();
    if (board_io_lock == NULL) {
        vQueueDelete(requests);
        requests = NULL;
        return ESP_ERR_NO_MEM;
    }
    if (!BZM_bridge_update_set_maintenance_hooks(bridge_acquire, bridge_release, NULL)) {
        vQueueDelete(requests);
        vSemaphoreDelete(board_io_lock);
        board_io_lock = NULL;
        requests = NULL;
        return ESP_FAIL;
    }
    STRATUM_V1_set_restart_guard(restart_guard, NULL);
    /* Above result dispatch (15), below the independent bridge service (18). */
    if (xTaskCreate(POWER_MANAGEMENT_task, "power management", 8192,
                    state, 16, &owner_task) != pdPASS) {
        vQueueDelete(requests);
        vSemaphoreDelete(board_io_lock);
        board_io_lock = NULL;
        requests = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void POWER_MANAGEMENT_set_ready(void)
{
    atomic_store_explicit(&ready, true, memory_order_release);
    POWER_MANAGEMENT_settings_changed();
}

bool POWER_MANAGEMENT_wait_started(uint32_t timeout_ms)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (!atomic_load_explicit(&boot_complete, memory_order_acquire)) {
        if (esp_timer_get_time() >= deadline) { revoke_work(); return false; }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return atomic_load_explicit(&boot_success, memory_order_acquire);
}

void POWER_MANAGEMENT_init_frequency(GlobalState *state)
{
    float frequency = state->SELF_TEST_MODULE.is_active
        ? state->DEVICE_CONFIG.family.asic.default_frequency_mhz
        : nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    state->POWER_MANAGEMENT_MODULE.frequency_value = frequency;
    state->POWER_MANAGEMENT_MODULE.actual_frequency = 50;
    state->POWER_MANAGEMENT_MODULE.expected_hashrate = frequency *
        state->DEVICE_CONFIG.family.asic.small_core_count * state->DEVICE_CONFIG.family.asic_count / 1000.0f;
}

static void process_request(power_request_t *request)
{
    bool cancelled = atomic_load_explicit(&request->state, memory_order_acquire) == REQUEST_CANCELLED;
    bool success = false;
    if (request->kind == POWER_REQUEST_PAUSE) {
        success = power_policy_pause(&policy);
    } else if (!cancelled || request->kind == POWER_REQUEST_RELEASE) {
        /* A release must drain even after its caller times out, otherwise an
         * update that has already finished would retain board ownership. */
        switch (request->kind) {
        case POWER_REQUEST_RESUME:
            if (request->epoch == atomic_load_explicit(&stop_epoch, memory_order_acquire)) {
                atomic_store_explicit(&accepted_epoch, request->epoch, memory_order_release);
                success = power_policy_resume(&policy);
            }
            break;
        case POWER_REQUEST_ACQUIRE:
            atomic_store_explicit(&maintenance, true, memory_order_release);
            success = power_policy_maintenance(&policy, request->owner, true);
            break;
        case POWER_REQUEST_RELEASE:
            success = power_policy_maintenance(&policy, request->owner, false);
            if (success) atomic_store_explicit(&maintenance, false, memory_order_release);
            break;
        default: break;
        }
    }
    request->success = success;
    int expected = REQUEST_WAITING;
    if (!atomic_compare_exchange_strong_explicit(&request->state, &expected,
            REQUEST_COMPLETE, memory_order_acq_rel, memory_order_acquire)) {
        if (success && request->kind == POWER_REQUEST_ACQUIRE)
            (void)power_policy_maintenance(&policy, request->owner, false);
        if (policy.owner == POWER_OWNER_NONE) (void)power_policy_pause(&policy);
    }
    atomic_store_explicit(&maintenance, policy.owner != POWER_OWNER_NONE, memory_order_release);
    xSemaphoreGive(request->completion);
    release_request(request);
}

void POWER_MANAGEMENT_task(void *parameter)
{
    GlobalState *state = parameter;
    xSemaphoreTake(board_io_lock, portMAX_DELAY);
    esp_err_t board_result = BoardPower_init(state, &board_operations);
    xSemaphoreGive(board_io_lock);
    power_operations_t operations = board_operations;
    operations.start = board_start;
    operations.stop = board_stop;
    operations.maintenance = board_maintenance;
    if (!power_policy_init(&policy, operations)) {
        ESP_LOGE(TAG, "Invalid board power operations");
        atomic_store(&boot_complete, true);
        vTaskDelete(NULL);
        return;
    }
    policy.fault = board_result != ESP_OK;
    atomic_store_explicit(&initialized, true, memory_order_release);
    for (;;) {
        policy.ready = atomic_load_explicit(&ready, memory_order_acquire);
        power_target_t target = {
            .voltage_mv = state->SELF_TEST_MODULE.is_active
                ? state->DEVICE_CONFIG.family.asic.default_voltage_mv
                : nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE),
            .frequency_mhz = state->SELF_TEST_MODULE.is_active
                ? state->DEVICE_CONFIG.family.asic.default_frequency_mhz
                : nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY),
        };
        policy.target = target;
        policy.pool_unavailable = state->SYSTEM_MODULE.pools_unavailable;
        power_request_t *request;
        while (xQueueReceive(requests, &request, 0) == pdTRUE) process_request(request);
        /* A failed enqueue or timed-out caller still revokes mining. */
        if (POWER_MANAGEMENT_stop_requested() && !policy.paused && policy.owner == POWER_OWNER_NONE)
            (void)power_policy_pause(&policy);
        /* No board access during maintenance, including SWD recovery. */
        power_sample_t sample = policy.owner == POWER_OWNER_NONE
            ? BoardPower_sample(state) : (power_sample_t){0};
        if (state->SELF_TEST_MODULE.is_finished) policy.paused = true;
        power_policy_step(&policy, (uint64_t)(esp_timer_get_time() / 1000), target, sample,
            state->SYSTEM_MODULE.pools_unavailable, state->SYSTEM_MODULE.hardware_fault,
            nvs_config_get_bool(NVS_CONFIG_OVERHEAT_MODE));
        state->SYSTEM_MODULE.mining_paused = policy.paused;
        atomic_store_explicit(&cooling, policy.cooling, memory_order_release);
        atomic_store_explicit(&fan_allowed, policy.running && !policy.fault && !policy.cooling, memory_order_release);
        if (policy.ready && (policy.running || policy.fault || policy.paused || policy.cooling)) {
            atomic_store_explicit(&boot_success, policy.running, memory_order_release);
            atomic_store_explicit(&boot_complete, true, memory_order_release);
        }
        if (policy.fault) {
            state->POWER_MANAGEMENT_MODULE.expected_hashrate = 0;
            if (!state->SYSTEM_MODULE.hardware_fault ||
                state->SYSTEM_MODULE.hardware_fault_msg[0] == '\0') {
                snprintf(state->SYSTEM_MODULE.hardware_fault_msg,
                    sizeof(state->SYSTEM_MODULE.hardware_fault_msg), "%.*s",
                    (int)sizeof(state->SYSTEM_MODULE.hardware_fault_msg) - 1,
                    sample.detail[0] ? sample.detail : "Board startup, shutdown or power transition failed");
            }
            state->SYSTEM_MODULE.hardware_fault = true;
        }
        /* Notifications only shorten this wait; hardware ownership never
         * moves to an HTTP, Stratum, fan, or bridge-update caller. */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_RATE_MS));
    }
}
