#include "../../stratum/test/job_test_state.h"
#include "global_state.h"
#include "asic.h"
#include "self_test.h"
#include "self_test_policy.h"
#include "unity.h"
#include <math.h>
#include <string.h>

static GlobalState state;
static asic_job_t jobs[4], attempts[8];
static asic_job_context_t contexts[4];
static unsigned sent, tried, delays, rejections;
static bool fail_create, cancel_rejected;
static uint16_t chip;
static double interval_ms;
static void (*created_task)(void *);

void self_test_fixture_worker(void *argument);
bool self_test_fixture_start(GlobalState *argument);
void self_test_fixture_stop(GlobalState *argument);
void self_test_fixture_submit(GlobalState *argument, const asic_job_t *job);

static void fake_delay(TickType_t ticks)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0, ticks);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(pdMS_TO_TICKS(100), ticks);
    TEST_ASSERT_LESS_THAN_UINT32(1000, ++delays);
    state.ASIC_initalized = true;
}

static bool send_work(GlobalState *argument, const asic_job_t *job)
{
    TEST_ASSERT_EQUAL_PTR(&state, argument);
    TEST_ASSERT_LESS_THAN_UINT32(8, tried);
    attempts[tried++] = *job;
    if (rejections > 0) {
        --rejections;
        if (cancel_rejected) self_test_fixture_stop(argument);
        return false;
    }
    TEST_ASSERT_LESS_THAN_UINT32(4, sent);
    jobs[sent] = *job;
    asic_work_handle_t handle;
    TEST_ASSERT_TRUE(asic_job_store_store_generated(&state.asic_job_store, job, &handle));
    asic_job_t saved;
    TEST_ASSERT_TRUE(asic_job_store_snapshot_with_context(&state.asic_job_store, handle, &saved, &contexts[sent]));
    ++sent;
    if (sent == 4) self_test_fixture_stop(argument);
    return true;
}

static const asic_driver_t *fixture_driver(int id)
{
    (void)id;
    static const asic_driver_t operations = {.ops.send_job = send_work};
    return &operations;
}

static bool pool_is_current(GlobalState *argument, uint64_t generation)
{
    (void)argument; (void)generation;
    TEST_FAIL_MESSAGE("Local self-test must not consult a pool session");
    return false;
}

#undef vTaskDelay
#define vTaskDelay fake_delay
#define ASIC_send_job self_test_fixture_submit
#define asic_driver_for_id fixture_driver
#define stratum_work_is_current pool_is_current
#include "../asic_submit.c"
#undef asic_driver_for_id
#undef stratum_work_is_current

static asic_capabilities_t fixture_capabilities(const GlobalState *argument)
{
    (void)argument;
    return ASIC_capabilities_for_chip_id(chip);
}
static double frequency(GlobalState *argument) { (void)argument; return interval_ms; }
static BaseType_t create_task(TaskFunction_t task, const char *name, uint32_t stack,
                              void *argument, UBaseType_t priority, TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)priority; (void)handle;
    TEST_ASSERT_EQUAL_PTR(&state, argument);
    created_task = task;
    return fail_create ? pdFAIL : pdPASS;
}
static void delete_task(TaskHandle_t handle) { TEST_ASSERT_NULL(handle); }
#undef xTaskCreate
#undef vTaskDelete
#define xTaskCreate create_task
#define vTaskDelete delete_task
#define ASIC_get_capabilities fixture_capabilities
#define ASIC_get_asic_job_frequency_ms frequency
#define self_test_start_work self_test_fixture_start
#define self_test_stop_work self_test_fixture_stop
#define self_test_work_task self_test_fixture_worker
#include "../../../main/self_test/self_test_work.c"
#undef self_test_start_work
#undef self_test_stop_work
#undef self_test_work_task
#undef ASIC_send_job

static void begin(uint16_t chip_id)
{
    memset(&state, 0, sizeof(state));
    memset(jobs, 0, sizeof(jobs));
    memset(attempts, 0, sizeof(attempts));
    sent = tried = delays = rejections = 0;
    fail_create = cancel_rejected = false;
    created_task = NULL;
    interval_ms = 0;
    chip = chip_id;
    state.ASIC_initalized = true;
    TEST_ASSERT_TRUE(asic_job_store_init(&state.asic_job_store));
}

TEST_CASE("Local self-test sends distinct BZM and Bitmain jobs without Stratum", "[asic][self-test]")
{
    const uint16_t chips[] = {0xB0A0, 1397, 1366};
    for (unsigned index = 0; index < sizeof(chips)/sizeof(chips[0]); ++index) {
        begin(chips[index]);
        TEST_ASSERT_TRUE(self_test_fixture_start(&state));
        TEST_ASSERT_FALSE(self_test_fixture_start(&state));
        TEST_ASSERT_NOT_NULL(created_task);
        created_task(&state);
        TEST_ASSERT_EQUAL_UINT32(4, sent);
        TEST_ASSERT_FALSE(atomic_load(&state.SELF_TEST_MODULE.worker_running));
        for (unsigned j = 0; j < sent; ++j) {
            TEST_ASSERT_TRUE(contexts[j].self_test);
            TEST_ASSERT_FALSE(contexts[j].pool_work);
            TEST_ASSERT_TRUE(contexts[j].work_generation == state.SELF_TEST_MODULE.work_generation);
            TEST_ASSERT_EQUAL_HEX32(0x647025b5 + j, jobs[j].ntime);
            TEST_ASSERT_EQUAL_HEX32(0x20000004 + (chips[index] == 1366 ? 0 : j * 0x8000), jobs[j].version);
            TEST_ASSERT_EQUAL_STRING("self-test", jobs[j].job_id);
            TEST_ASSERT_EQUAL_DOUBLE(0, jobs[j].pool_diff);
        }
        TEST_ASSERT_GREATER_THAN_UINT32(0, delays);
        asic_job_store_destroy(&state.asic_job_store);
    }
}

TEST_CASE("Local self-test retries the identical borrowed work and cancels rejected work", "[asic][self-test]")
{
    begin(0xB0A0);
    rejections = 1;
    TEST_ASSERT_TRUE(self_test_fixture_start(&state));
    created_task(&state);
    TEST_ASSERT_EQUAL_MEMORY(&attempts[0], &attempts[1], sizeof(asic_job_t));
    TEST_ASSERT_EQUAL_UINT32(4, sent);
    asic_job_store_destroy(&state.asic_job_store);
    begin(0xB0A0);
    rejections = 1;
    cancel_rejected = true;
    TEST_ASSERT_TRUE(self_test_fixture_start(&state));
    created_task(&state);
    TEST_ASSERT_EQUAL_UINT32(1, tried);
    TEST_ASSERT_EQUAL_UINT32(0, sent);
    TEST_ASSERT_FALSE(atomic_load(&state.SELF_TEST_MODULE.worker_running));
    asic_job_store_destroy(&state.asic_job_store);
}

TEST_CASE("Local self-test task failure stale epochs and exhaustion revoke admission", "[asic][self-test]")
{
    begin(0xB0A0);
    fail_create = true;
    TEST_ASSERT_FALSE(self_test_fixture_start(&state));
    TEST_ASSERT_FALSE(state.asic_job_store.local_active);
    TEST_ASSERT_FALSE(atomic_load(&state.SELF_TEST_MODULE.worker_running));
    fail_create = false;
    TEST_ASSERT_FALSE(self_test_fixture_start(NULL));
    TEST_ASSERT_TRUE(self_test_fixture_start(&state));
    asic_job_store_cancel_local(&state.asic_job_store);
    created_task(&state);
    TEST_ASSERT_TRUE(atomic_load(&state.SELF_TEST_MODULE.worker_failed));
    TEST_ASSERT_EQUAL_UINT32(0, sent);
    state.asic_job_store.local_generation = UINT64_MAX;
    TEST_ASSERT_FALSE(self_test_fixture_start(&state));
    TEST_ASSERT_FALSE(state.asic_job_store.local_active);
    asic_job_store_destroy(&state.asic_job_store);
}

TEST_CASE("Local self-test always yields and bounds long or invalid waits", "[asic][self-test]")
{
    const double intervals[] = {0, NAN, -1, 60001, 501};
    for (unsigned i = 0; i < sizeof(intervals)/sizeof(intervals[0]); ++i) {
        begin(0xB0A0);
        interval_ms = intervals[i];
        state.ASIC_initalized = false;
        TEST_ASSERT_TRUE(self_test_fixture_start(&state));
        created_task(&state);
        TEST_ASSERT_EQUAL_UINT32(4, sent);
        TEST_ASSERT_GREATER_THAN_UINT32(0, delays);
        asic_job_store_destroy(&state.asic_job_store);
    }
}
