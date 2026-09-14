#include <errno.h>
#include <inttypes.h>
#include <lwip/tcpip.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "asic.h"
#include "asic_result_handler.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "scoreboard.h"
#include "self_test.h"
#include "sv1_client.h"
#include "stratum_task.h"
#include "sv2_protocol.h"
#include "system.h"

static const char *TAG = "asic_result";

typedef struct {
    GlobalState *state;
} result_callback_context;

static void monitor_register(GlobalState *state,
                             const asic_register_result_t *result)
{
    hashrate_monitor_register_read(state, result->register_type,
                                   result->asic_index,
                                   result->value, result->timestamp_us);
}

static void record_self_test(void *context, const asic_share_submission_t *share)
{
    result_callback_context *callback_context = context;
    self_test_record_nonce(callback_context->state, share->nonce_diff);
    ASIC_record_local_result(callback_context->state, share->result->asic_index,
                              share->result->engine_id, true, share->nonce_diff);
}

static int submit_share(void *context, const asic_share_submission_t *share)
{
    GlobalState *state = ((result_callback_context *)context)->state;
    uint64_t sent_time_us = 0;
    int ret = stratum_submit_share(state, share, &sent_time_us);
    if (ret >= 0 && sent_time_us >= share->result->timestamp_us) {
        state->SYSTEM_MODULE.process_time =
            (sent_time_us - share->result->timestamp_us) / 1000.0f;
        ESP_LOGD(TAG, "Processing time: %0.1f ms", state->SYSTEM_MODULE.process_time);
    }
    return ret;
}

static void account_share(void *context,
                          const asic_share_submission_t *share)
{
    GlobalState *state = ((result_callback_context *)context)->state;
    const asic_result_t *result = share->result;
    ESP_LOGD(TAG,
             "ID: %s, ASIC nr: %d, Core: %d/%d, ver: %08" PRIX32
             " Nonce %08" PRIX32 " diff %.1f of %g.",
             share->job_id, result->asic_index, result->core_id,
             result->small_core_id, result->final_version, result->nonce,
             share->nonce_diff, share->pool_difficulty);

    SYSTEM_notify_found_nonce(state, share->nonce_diff, share->target);
    scoreboard_add(&state->SYSTEM_MODULE.scoreboard, share->nonce_diff,
                   share->job_id, share->extranonce2, share->ntime,
                   share->nonce, share->version_bits);
    ASIC_record_local_result(state, result->asic_index, result->engine_id, true,
                             share->nonce_diff);
}

static const asic_result_callbacks_t RESULT_CALLBACKS = {
    .record_self_test = record_self_test,
    .submit_sv1 = submit_share,
    .submit_sv2_standard = submit_share,
    .submit_sv2_extended = submit_share,
    .account_share = account_share,
};

static bool work_is_current(void *context, uint64_t generation)
{
    return stratum_work_is_current(
        ((result_callback_context *)context)->state, generation);
}

static asic_result_status_t handle_result(GlobalState *state,
                                          const asic_result_t *result)
{
    result_callback_context callback_context = {.state = state};
    uint16_t active_pool_index = state->SYSTEM_MODULE.is_using_fallback
        ? state->SYSTEM_MODULE.secondary_pool_index
        : state->SYSTEM_MODULE.primary_pool_index;
    asic_result_context_t context = {
        .job_store = &state->asic_job_store,
        .username = state->SYSTEM_MODULE.pools[active_pool_index].user,
        .callback_context = &callback_context,
        .work_is_current = work_is_current,
    };
    return asic_result_handle(result, &context, &RESULT_CALLBACKS);
}

void ASIC_result_task(void *pvParameters)
{
    GlobalState *state = pvParameters;

    while (1) {
        if (!state->ASIC_initalized) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        asic_event_t *event = ASIC_process_work(state);
        if (event == NULL) continue;

        if (event->type == ASIC_EVENT_REGISTER_RESULT) {
            monitor_register(state, &event->data.register_result);
            continue;
        }

        if (event->type != ASIC_EVENT_SHARE_RESULT) {
            ESP_LOGW(TAG, "Ignoring unknown ASIC event type %d", event->type);
            continue;
        }

        const asic_result_t *result = &event->data.share;
        asic_result_status_t status = handle_result(state, result);
        if (status == ASIC_RESULT_REJECTED_WORK) {
            ASIC_record_local_result(state, result->asic_index,
                                     result->engine_id, false, 0.0);
            ESP_LOGW(TAG, "Invalid work result found, 0x%" PRIX64,
                     result->work_handle);
        }
    }
}
