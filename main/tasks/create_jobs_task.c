#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>

#include "global_state.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mining.h"
#include "miner_job.h"
#include "string.h"
#include "esp_timer.h"

#include "asic.h"
#include "system.h"
#include "esp_heap_caps.h"
#include "utils.h"
#include "stratum_task.h"

static const char *TAG = "create_jobs_task";

static bool generate_work_from_miner_job(GlobalState *state, const miner_job_t *job,
                                         uint64_t extranonce2, uint32_t version,
                                         bool clean_jobs)
{
    if (!state->ASIC_initalized ||
        !stratum_work_is_current(state, job->work_generation)) return false;
    const asic_job_context_t context = {
        .work_generation = job->work_generation,
        .job_version = job->version,
        .clean_jobs = clean_jobs,
        .pool_work = true,
    };
    asic_job_t template;
    if (!mining_build_asic_job(job, extranonce2, version, &template)) {
        ESP_LOGE(TAG, "Unable to materialize pool job");
        return false;
    }
    asic_job_store_begin_submission(&state->asic_job_store, &template, &context);
    ASIC_send_job(state, &template);
    asic_job_store_end_submission(&state->asic_job_store);
    return stratum_work_is_current(state, context.work_generation);
}

void create_jobs_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    uint32_t current_version_mask = 0;
    miner_job_t *current_work = NULL;
    bool current_work_sent = false;
    bool clean_jobs_pending = false;
    uint64_t current_work_generation = UINT64_MAX;
    uint64_t extranonce_2 = 0;
    uint32_t current_version = 0;
    asic_capabilities_t capabilities = ASIC_get_capabilities(GLOBAL_STATE);
    int timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);

    ESP_LOGI(TAG, "ASIC Job Interval: %d ms", timeout_ms);
    ESP_LOGI(TAG, "ASIC Ready!");

    while (1) {
        uint64_t start_time = esp_timer_get_time();
        uint32_t slot_notify = 0;
        TickType_t wait_ticks = (timeout_ms > 0) ? pdMS_TO_TICKS(timeout_ms) : 0;
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &slot_notify, wait_ticks);
        timeout_ms -= (esp_timer_get_time() - start_time) / 1000;

        if (notified == pdTRUE) {
            miner_job_t *new_work = miner_job_get_slot((size_t)slot_notify);
            ESP_LOGI(TAG, "New Work Activated (slot %lu) %s (type %d)", (unsigned long)slot_notify, new_work->job_id, new_work->type);
            current_work = new_work;
            GLOBAL_STATE->active_job_slot_idx = (uint8_t)(slot_notify % MINER_JOB_POOL_SIZE);
            current_work_sent = false;
            // Preserve a clean boundary even when a later non-clean notify
            // overwrites the task notification before this task consumes it.
            clean_jobs_pending = new_work->clean_jobs ||
                new_work->work_generation != current_work_generation;
            current_work_generation = new_work->work_generation;
            GLOBAL_STATE->SYSTEM_MODULE.last_work_received_us = esp_timer_get_time();
            current_version = new_work->version;

            if (new_work->version_mask != current_version_mask && GLOBAL_STATE->ASIC_initalized) {
                ESP_LOGI(TAG, "Set chip version rolls %i", (int)(new_work->version_mask >> 13));
                ASIC_set_version_mask(GLOBAL_STATE, new_work->version_mask);
                current_version_mask = new_work->version_mask;
            }

            extranonce_2 = 0;

            if (!clean_jobs_pending) {
                // Staged job for next cycle, let current ASIC cycle finish
                continue;
            }
        } else {
            if (current_work == NULL) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            }
            if (!miner_job_is_rollable(current_work) && current_work_sent &&
                ASIC_capabilities_support_static_work(&capabilities)) {
                timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
                continue;
            }
        }

        bool sent = generate_work_from_miner_job(GLOBAL_STATE, current_work,
                                                  extranonce_2, current_version,
                                                  clean_jobs_pending);
        if (!sent) {
            timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
            continue;
        }
        clean_jobs_pending = false;
        if (!current_work_sent) {
            SYSTEM_decode_and_apply_coinbase(GLOBAL_STATE, current_work);
        }
        current_work_sent = true;

        if (miner_job_is_rollable(current_work)) {
            extranonce_2++;
        } else if (capabilities.version_rolling == ASIC_VERSION_ROLLING_MIDSTATE) {
            // Finite-midstate drivers (BM1397 and BZM) need fresh base versions
            // when the pool supplies a fixed merkle root. Advance only after
            // accepted work and only within the pool's negotiated mask.
            uint32_t mask = current_work->version_mask & capabilities.supported_version_mask;
            for (uint32_t i = 0; i < capabilities.max_version_variants; i++) {
                current_version = increment_bitmask(current_version, mask);
            }
        }
        timeout_ms = ASIC_get_asic_job_frequency_ms(GLOBAL_STATE);
    }
}
