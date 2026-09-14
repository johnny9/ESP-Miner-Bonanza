#include "global_state.h"
#include "self_test.h"
#include "self_test_policy.h"
#include "asic.h"
#include "freertos/task.h"
#include "mining.h"
#include <math.h>

static void self_test_work_delay(SelfTestModule *test, double interval_ms)
{
    if (!isfinite(interval_ms) || interval_ms < 0 || interval_ms > 60000)
        interval_ms = 100;
    TickType_t remaining = pdMS_TO_TICKS((uint32_t)interval_ms);
    if (remaining == 0) remaining = 1;
    const TickType_t maximum = pdMS_TO_TICKS(100) > 0 ? pdMS_TO_TICKS(100) : 1;
    while (remaining > 0 && !atomic_load(&test->worker_stop) &&
           !atomic_load(&test->cancel_requested)) {
        TickType_t part = remaining > maximum ? maximum : remaining;
        vTaskDelay(part);
        remaining -= part;
    }
}

void self_test_work_task(void *argument)
{
    GlobalState *state = argument;
    SelfTestModule *test = &state->SELF_TEST_MODULE;
    asic_job_t job;
    self_test_build_job(&job);
    const asic_capabilities_t capabilities = ASIC_get_capabilities(state);
    job.version_mask &= capabilities.supported_version_mask;
    const asic_job_context_t context = {
        .self_test = true, .job_version = job.version,
        .work_generation = test->work_generation,
    };
    while (!atomic_load(&test->worker_stop) && !atomic_load(&test->cancel_requested)) {
        if (!asic_job_store_local_is_current(&state->asic_job_store,
                                              context.work_generation)) break;
        asic_job_store_begin_submission(&state->asic_job_store, &job, &context);
        ASIC_send_job(state, &job);
        asic_job_store_end_submission(&state->asic_job_store);
        if (!asic_job_store_local_is_current(&state->asic_job_store,
                                              context.work_generation)) break;
        /* Advance only after the void adapter accepts this borrowed job.
         * Distinct time and finite-midstate ranges prevent repeated headers. */
        if (job.ntime == UINT32_MAX) {
            atomic_store(&test->worker_failed, true);
            break;
        }
        ++job.ntime;
        if (capabilities.version_rolling == ASIC_VERSION_ROLLING_MIDSTATE) {
            for (uint32_t i = 0; i < capabilities.max_version_variants; ++i)
                job.version = increment_bitmask(job.version, job.version_mask);
        }
        self_test_work_delay(test, ASIC_get_asic_job_frequency_ms(state));
    }
    if (!atomic_load(&test->worker_stop) && !atomic_load(&test->cancel_requested))
        atomic_store(&test->worker_failed, true);
    atomic_store(&test->worker_running, false);
    vTaskDelete(NULL);
}

bool self_test_start_work(GlobalState *state)
{
    if (state == NULL || state->asic_job_store.entries == NULL) return false;
    SelfTestModule *test = &state->SELF_TEST_MODULE;
    if (atomic_exchange(&test->worker_running, true)) return false;
    test->work_generation = asic_job_store_activate_local(&state->asic_job_store);
    if (test->work_generation == 0) {
        atomic_store(&test->worker_running, false);
        return false;
    }
    atomic_store(&test->worker_stop, false);
    atomic_store(&test->worker_failed, false);
    if (xTaskCreate(self_test_work_task, "self-test work", 8192, state, 20, NULL) != pdPASS) {
        asic_job_store_cancel_local(&state->asic_job_store);
        atomic_store(&test->worker_running, false);
        return false;
    }
    return true;
}

void self_test_stop_work(GlobalState *state)
{
    atomic_store(&state->SELF_TEST_MODULE.worker_stop, true);
    if (state->asic_job_store.entries != NULL)
        asic_job_store_cancel_local(&state->asic_job_store);
}
