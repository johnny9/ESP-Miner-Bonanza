#include "stratum_submission.h"

#include <inttypes.h>
#include "asic_share.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "global_state.h"
#include "stratum_task.h"

static const char *TAG = "stratum_submit";

bool stratum_submission_init(GlobalState *state)
{
    if (!state || state->stratum_share_queue) return false;
    state->stratum_share_queue = xQueueCreateWithCaps(STRATUM_SHARE_QUEUE_LENGTH,
        sizeof(asic_share_snapshot_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return state->stratum_share_queue != NULL;
}

int stratum_queue_share(GlobalState *state, const asic_share_submission_t *share)
{
    asic_share_snapshot_t snapshot;
    if (!state || !state->stratum_share_queue ||
        !asic_share_snapshot_capture(&snapshot, share)) return -1;
    if (xQueueSend(state->stratum_share_queue, &snapshot, 0) != pdTRUE) {
        /* One producer owns this counter. Bound log volume during an outage. */
        if ((state->stratum_share_drops++ % 32U) == 0)
            ESP_LOGW(TAG, "Share queue full; dropped network submissions: %" PRIu32,
                     state->stratum_share_drops);
        return -1;
    }
    return 0;
}

void stratum_submission_task(void *context)
{
    GlobalState *state = context;
    asic_share_snapshot_t snapshot;
    while (1) {
        if (xQueueReceive(state->stratum_share_queue, &snapshot, portMAX_DELAY) != pdTRUE)
            continue;
        asic_share_submission_t share = asic_share_snapshot_view(&snapshot);
        if (!stratum_work_is_current(state, share.work_generation)) continue;
        /* Each protocol rechecks session/generation under the transport lock
         * immediately before writing. No result/queue lock spans this call. */
        uint64_t sent_time_us = 0;
        int ret = stratum_submit_share(state, &share, &sent_time_us);
        if (ret >= 0 && sent_time_us >= snapshot.result.timestamp_us) {
            state->SYSTEM_MODULE.process_time =
                (sent_time_us - snapshot.result.timestamp_us) / 1000.0f;
            ESP_LOGD(TAG, "Processing time: %0.1f ms", state->SYSTEM_MODULE.process_time);
        }
    }
}
