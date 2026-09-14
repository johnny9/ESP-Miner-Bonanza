#include "job_test_state.h"
#include "mining_allocator_fault_injector.h"
#include "mining_job.h"
#include "unity.h"
#include "job_pipeline_test_harness.h"

#include <setjmp.h>
#include <string.h>

#include "asic.h"
#include "global_state.h"
#include "system.h"

_Static_assert(_Generic(&ASIC_send_job,
    void (*)(GlobalState *, const asic_job_t *): 1, default: 0),
    "ASIC_send_job must retain the stage 03 submission signature");

#include "../../../main/tasks/create_jobs_task.h"

static jmp_buf harness_exit;
static const job_pipeline_harness_event_t *harness_events;
static size_t harness_event_count;
static size_t harness_event_index;
static job_pipeline_harness_result_t *harness_result;
static int harness_job_frequency_ms;
static unsigned harness_failed_sends;
static bool harness_retire_during_retry;
static uint16_t harness_chip_id;
static uint64_t harness_current_generation;
static GlobalState harness_state;

static BaseType_t fake_task_notify_wait(
    uint32_t bits_to_clear_on_entry, unsigned long bits_to_clear_on_exit,
    uint32_t *notification_value, TickType_t ticks_to_wait)
{
    (void)bits_to_clear_on_entry;
    (void)bits_to_clear_on_exit;
    (void)ticks_to_wait;

    if (harness_event_index >= harness_event_count) {
        longjmp(harness_exit, 1);
    }

    const job_pipeline_harness_event_t *event =
        &harness_events[harness_event_index++];
    if (event->type == JOB_PIPELINE_HARNESS_NOTIFY) {
        *notification_value = event->slot;
        return pdTRUE;
    }
    return pdFALSE;
}

static void spy_task_delay(TickType_t ticks)
{
    (void)ticks;
    harness_result->delay_count++;
    if (harness_retire_during_retry && harness_result->send_attempts > 0)
        harness_current_generation++;
}

static bool spy_asic_send_work(GlobalState *state, const asic_job_t *job)
{
    TEST_ASSERT_LESS_THAN_UINT(JOB_PIPELINE_HARNESS_MAX_JOBS, harness_result->send_attempts);
    size_t attempt = harness_result->send_attempts++;
    harness_result->attempted_jobs[attempt] = *job;
    asic_job_store_submission_context(&state->asic_job_store, job,
                                      &harness_result->attempted_contexts[attempt]);
    if (harness_failed_sends > 0) {
        harness_failed_sends--;
        return false;
    }
    TEST_ASSERT_LESS_THAN_UINT(JOB_PIPELINE_HARNESS_MAX_JOBS, harness_result->job_count);
    asic_job_t *owned = malloc(sizeof(*owned));
    TEST_ASSERT_NOT_NULL(owned);
    *owned = *job;
    harness_result->contexts[harness_result->job_count] =
        harness_result->attempted_contexts[attempt];
    harness_result->jobs[harness_result->job_count++] = owned;
    return true;
}

static bool fake_work_is_current(GlobalState *state, uint64_t generation)
{
    (void)state;
    return generation == harness_current_generation;
}

static asic_capabilities_t fake_capabilities(const GlobalState *state)
{
    return ASIC_capabilities_for_chip_id(
        harness_chip_id != 0 ? harness_chip_id :
        (state->DEVICE_CONFIG.family.asic.hardware_version_rolling ? 1366 : 1397));
}

bool mining_test_template_build_miner_job(const miner_job_t *job, uint64_t extranonce2,
                                         uint32_t version, asic_job_t *work);

static void spy_asic_set_version_mask(GlobalState *state, uint32_t mask)
{
    (void)state;
    if (harness_result->version_mask_count >= JOB_PIPELINE_HARNESS_MAX_JOBS) {
        longjmp(harness_exit, 2);
    }
    harness_result->version_masks[harness_result->version_mask_count++] = mask;
}

static double stub_asic_get_job_frequency(GlobalState *state)
{
    (void)state;
    return harness_job_frequency_ms;
}

static void spy_decode_coinbase(GlobalState *state, const miner_job_t *job)
{
    (void)state;
    (void)job;
    harness_result->coinbase_decode_count++;
}

static const asic_driver_t *fake_submission_driver(int id)
{
    (void)id;
    static const asic_driver_t driver = {.ops.send_job = spy_asic_send_work};
    return &driver;
}

/* Exercise the real void adapter, including backpressure and cancellation. */
void job_pipeline_test_asic_send_job(GlobalState *state, const asic_job_t *job);
#define ASIC_send_job job_pipeline_test_asic_send_job
#define asic_driver_for_id fake_submission_driver
#define stratum_work_is_current fake_work_is_current
#ifdef vTaskDelay
#undef vTaskDelay
#endif
#define vTaskDelay spy_task_delay
#include "../../asic/asic_submit.c"
#undef asic_driver_for_id
#undef stratum_work_is_current
#undef vTaskDelay
#undef ASIC_send_job

/* Compile the whole task with only scheduling, driver and session boundaries
 * replaced. The common builder below is the complete fault-injected instance. */
#ifdef xTaskNotifyWait
#undef xTaskNotifyWait
#endif
#ifdef vTaskDelay
#undef vTaskDelay
#endif
#define stratum_work_is_current fake_work_is_current
#define ASIC_get_capabilities fake_capabilities
#define mining_build_asic_job mining_test_template_build_miner_job
void job_pipeline_test_create_jobs_task(void *context);
#define create_jobs_task job_pipeline_test_create_jobs_task
#define xTaskNotifyWait fake_task_notify_wait
#define vTaskDelay spy_task_delay
#define ASIC_send_job job_pipeline_test_asic_send_job
#define ASIC_set_version_mask spy_asic_set_version_mask
#define ASIC_get_asic_job_frequency_ms stub_asic_get_job_frequency
#define SYSTEM_decode_and_apply_coinbase spy_decode_coinbase
#include "../../../main/tasks/create_jobs_task.c"
#undef SYSTEM_decode_and_apply_coinbase
#undef ASIC_get_asic_job_frequency_ms
#undef ASIC_set_version_mask
#undef ASIC_send_job
#undef vTaskDelay
#undef xTaskNotifyWait

void job_pipeline_harness_run(
    job_pipeline_harness_config_t config,
    const job_pipeline_harness_event_t *events, size_t event_count,
    job_pipeline_harness_result_t *result)
{
    if (result == NULL || event_count > JOB_PIPELINE_HARNESS_MAX_EVENTS ||
        (event_count > 0 && events == NULL)) {
        return;
    }

    memset(result, 0, sizeof(*result));
    harness_state = (GlobalState) {
        .DEVICE_CONFIG.family.asic.hardware_version_rolling =
            config.hardware_version_rolling,
        .DEVICE_CONFIG.family.asic.software_midstates =
            config.software_midstates,
        .ASIC_initalized = config.asic_initialized,
    };

    harness_events = events;
    harness_event_count = event_count;
    harness_event_index = 0;
    harness_result = result;
    harness_job_frequency_ms = config.job_frequency_ms;
    harness_failed_sends = config.failed_sends;
    harness_retire_during_retry = config.retire_during_retry;
    TEST_ASSERT_TRUE(asic_job_store_init(&harness_state.asic_job_store));
    harness_chip_id = config.chip_id;
    harness_current_generation = config.current_generation;
    mining_allocator_fault_injector_reset(config.allocation_failure_at);

    int exit_reason = setjmp(harness_exit);
    if (exit_reason == 0) {
        create_jobs_task(&harness_state);
    }

    asic_job_store_destroy(&harness_state.asic_job_store);
    result->active_job_slot = harness_state.active_job_slot_idx;
    result->allocation_count = mining_allocator_fault_injector_calls();
    harness_events = NULL;
    harness_event_count = 0;
    harness_event_index = 0;
    harness_result = NULL;
    mining_allocator_fault_injector_reset(0);
}

void job_pipeline_harness_result_free(job_pipeline_harness_result_t *result)
{
    if (result == NULL) return;
    for (size_t index = 0; index < result->job_count; ++index) {

        free(result->jobs[index]);
        result->jobs[index] = NULL;
    }
    result->job_count = 0;
}
