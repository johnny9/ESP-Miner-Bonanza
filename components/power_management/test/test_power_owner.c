#include "unity.h"
#include <stdatomic.h>
#include "board_power.h"
#include "global_state.h"
#include "nvs_config.h"
#include "power_management_task.h"
#include "freertos/task.h"

/* Link-time board and settings ports for the real FreeRTOS power task.
 * No ASIC sequencing or power policy is reproduced in these fixtures. */
static GlobalState state;
static TaskHandle_t board_owner;
static SemaphoreHandle_t operation_release;
static atomic_uint starts, stops, releases, samples;
static atomic_bool wrong_owner, block_start, block_maintenance;
static atomic_uint voltage;
static atomic_uint frequency;
static atomic_bool overheat;

static void check_owner(void)
{
    if (xTaskGetCurrentTaskHandle() != board_owner) atomic_store(&wrong_owner, true);
}
static power_start_result_t owner_start(void *context)
{
    (void)context; check_owner(); atomic_fetch_add(&starts, 1);
    if (atomic_load(&block_start)) xSemaphoreTake(operation_release, portMAX_DELAY);
    return POWER_START_OK;
}
static bool owner_stop(void *context)
{
    (void)context; check_owner(); atomic_fetch_add(&stops, 1); return true;
}
static bool owner_apply(void *context, power_target_t target)
{
    (void)context; (void)target; check_owner(); return true;
}
static bool owner_maintenance(void *context, power_owner_t owner, bool acquire)
{
    (void)context; (void)owner; check_owner();
    if (acquire && atomic_load(&block_maintenance)) xSemaphoreTake(operation_release, portMAX_DELAY);
    if (!acquire) atomic_fetch_add(&releases, 1);
    return true;
}
static void owner_overheat(void *context, bool enabled)
{
    (void)context; check_owner(); atomic_store(&overheat, enabled);
}
static bool owner_save(void *context, power_target_t target)
{
    (void)context; check_owner(); atomic_store(&voltage, target.voltage_mv);
    atomic_store(&frequency, (unsigned)target.frequency_mhz); return true;
}
static bool owner_cancelled(void *context) { (void)context; return POWER_MANAGEMENT_stop_requested(); }

esp_err_t BoardPower_init(GlobalState *global, power_operations_t *operations)
{
    board_owner = xTaskGetCurrentTaskHandle();
    *operations = (power_operations_t){
        .context = global, .start = owner_start, .stop = owner_stop,
        .apply = owner_apply, .maintenance = owner_maintenance,
        .overheat = owner_overheat, .save_target = owner_save, .cancelled = owner_cancelled,
        .minimum_voltage_mv = 2100, .minimum_frequency_mhz = 800,
    };
    return ESP_OK;
}
power_sample_t BoardPower_sample(GlobalState *global)
{
    (void)global; check_owner(); atomic_fetch_add(&samples, 1);
    return (power_sample_t){.health = POWER_HEALTH_OK, .vreg_valid = true, .vreg_c = 60};
}
uint16_t nvs_config_get_u16(NvsConfigKey key) { (void)key; return (uint16_t)atomic_load(&voltage); }
float nvs_config_get_float(NvsConfigKey key) { (void)key; return (float)atomic_load(&frequency); }
bool nvs_config_get_bool(NvsConfigKey key) { (void)key; return atomic_load(&overheat); }

static bool wait_count(atomic_uint *counter, unsigned above)
{
    for (unsigned i = 0; i < 100; ++i) {
        if (atomic_load(counter) > above) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

TEST_CASE("Power task owns commands and cleans up timed out hardware operations", "[power-management][qemu-integration]")
{
    atomic_store(&voltage, 2800); atomic_store(&frequency, 1200);
    operation_release = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(operation_release);
    TEST_ASSERT_EQUAL(ESP_OK, POWER_MANAGEMENT_init(&state));
    POWER_MANAGEMENT_set_ready();
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_wait_started(2000));
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_pause());

    unsigned previous_stops = atomic_load(&stops);
    atomic_store(&block_start, true);
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_request(POWER_REQUEST_RESUME, POWER_OWNER_NONE, 30));
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_stop_requested());
    xSemaphoreGive(operation_release);
    TEST_ASSERT_TRUE(wait_count(&stops, previous_stops));
    atomic_store(&block_start, false);
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_resume());

    TEST_ASSERT_TRUE(POWER_MANAGEMENT_acquire_maintenance(POWER_OWNER_OTA));
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_in_maintenance());
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_board_io_begin());
    unsigned previous_samples = atomic_load(&samples);
    vTaskDelay(pdMS_TO_TICKS(220));
    TEST_ASSERT_EQUAL_UINT(previous_samples, atomic_load(&samples));
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_acquire_maintenance(POWER_OWNER_BRIDGE));
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_release_maintenance(POWER_OWNER_OTA));
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_resume());

    TEST_ASSERT_TRUE(POWER_MANAGEMENT_acquire_maintenance(POWER_OWNER_BRIDGE));
    vTaskDelay(pdMS_TO_TICKS(110));
    vTaskSuspend(board_owner);
    unsigned queued_release = atomic_load(&releases);
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_request(POWER_REQUEST_RELEASE, POWER_OWNER_BRIDGE, 30));
    vTaskResume(board_owner);
    TEST_ASSERT_TRUE(wait_count(&releases, queued_release));
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_in_maintenance());
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_resume());

    unsigned previous_releases = atomic_load(&releases);
    atomic_store(&block_maintenance, true);
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_request(POWER_REQUEST_ACQUIRE, POWER_OWNER_OTA, 30));
    xSemaphoreGive(operation_release);
    TEST_ASSERT_TRUE(wait_count(&releases, previous_releases));
    atomic_store(&block_maintenance, false);
    TEST_ASSERT_FALSE(POWER_MANAGEMENT_in_maintenance());
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_resume());
    TEST_ASSERT_TRUE(POWER_MANAGEMENT_pause());
    TEST_ASSERT_FALSE(atomic_load(&wrong_owner));
    vTaskDelete(board_owner);
    vSemaphoreDelete(operation_release);
}
