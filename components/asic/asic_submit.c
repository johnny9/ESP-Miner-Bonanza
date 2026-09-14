#include "asic.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stratum_task.h"

void ASIC_send_job(GlobalState *state, const asic_job_t *job)
{
    if (state == NULL || job == NULL) return;
    const asic_driver_t *driver = asic_driver_for_id(state->DEVICE_CONFIG.family.asic.id);
    if (driver == NULL || driver->ops.send_job == NULL) {
        ESP_LOGE("asic", "No work operation for ASIC id %d", state->DEVICE_CONFIG.family.asic.id);
        return;
    }

    asic_job_context_t context = {.job_version = job->version};
    asic_job_store_submission_context(&state->asic_job_store, job, &context);
    /* The borrowed job stays alive for this call. Retrying here applies
     * backpressure without changing the common void submission interface.
     * A new pool generation cancels the entire pending assignment, including
     * its clean boundary; the producer will consume the new notification. */
    for (;;) {
        if (context.pool_work &&
            !stratum_work_is_current(state, context.work_generation)) return;
        if (state->ASIC_initalized && driver->ops.send_job(state, job)) return;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
