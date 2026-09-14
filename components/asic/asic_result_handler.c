#include "asic_result_handler.h"

#include <string.h>
#include <stdlib.h>
#include "mining.h"
#include "utils.h"

asic_result_status_t asic_result_handle(
    const asic_result_t *result, const asic_result_context_t *context,
    const asic_result_callbacks_t *callbacks)
{
    if (result == NULL || context == NULL || callbacks == NULL ||
        context->job_store == NULL) {
        return ASIC_RESULT_REJECTED_WORK;
    }

    asic_job_t template;
    asic_job_context_t provenance;
    if (!asic_job_store_snapshot_with_context(context->job_store, result->work_handle,
                                              &template, &provenance)) {
        return ASIC_RESULT_STALE_WORK;
    }

    bool metadata_valid =
        template.source_type >= JOB_TYPE_V1 &&
        template.source_type <= JOB_TYPE_SV2_EXTENDED &&
        memchr(template.job_id, 0, sizeof(template.job_id)) != NULL &&
        memchr(template.extranonce2, 0, sizeof(template.extranonce2)) != NULL &&
        result->version_bits ==
            (result->final_version ^ template.version) &&
        (result->version_bits & ~template.version_mask) == 0;
    if (!metadata_valid) {
        return ASIC_RESULT_REJECTED_WORK;
    }

    asic_share_submission_t share = {
        .result = result,
        .username = context->username,
        .job_id = template.job_id,
        .extranonce2 = template.extranonce2,
        .extranonce2_len = strlen(template.extranonce2) / 2,
        .numeric_job_id = (uint32_t)strtoul(template.job_id, NULL, 10),
        .nonce = result->nonce,
        .nonce_diff = mining_test_nonce_value(
            &template, result->nonce, result->final_ntime,
            result->final_version),
        .ntime = result->final_ntime,
        .base_version = template.version,
        .final_version = result->final_version,
        .version_bits = result->version_bits,
        .pool_difficulty = template.pool_diff,
        .target = template.nbits,
        .protocol = template.source_type,
        .pool_id = template.pool_id,
        .work_generation = provenance.work_generation,
        .job_version = provenance.job_version,
    };
    if (strlen(template.extranonce2) % 2 != 0 ||
        hex2bin(template.extranonce2, share.extranonce2_bin,
                sizeof(share.extranonce2_bin)) != share.extranonce2_len)
        return ASIC_RESULT_REJECTED_WORK;

    if (provenance.self_test) {
        if (!asic_job_store_local_is_current(context->job_store,
                                              provenance.work_generation)) {
            return ASIC_RESULT_STALE_WORK;
        }
        if (callbacks->record_self_test != NULL) {
            callbacks->record_self_test(context->callback_context,
                                        &share);
        }

        return ASIC_RESULT_RECORDED_SELF_TEST;
    }

    if (!asic_job_store_contains(context->job_store, result->work_handle) ||
        (context->work_is_current != NULL &&
         !context->work_is_current(context->callback_context, share.work_generation))) {
        return ASIC_RESULT_STALE_WORK;
    }

    if (share.pool_difficulty > 0.0 && share.nonce_diff >= share.pool_difficulty) {
        switch (share.protocol) {
            case JOB_TYPE_V1: {
                bool ready = callbacks->sv1_transport_ready == NULL ||
                    callbacks->sv1_transport_ready(
                        context->callback_context, &share);
                if (ready && callbacks->submit_sv1 != NULL) {
                    callbacks->submit_sv1(context->callback_context, &share);
                }
                break;
            }
            case JOB_TYPE_SV2_STANDARD:
                if (callbacks->submit_sv2_standard != NULL) {
                    callbacks->submit_sv2_standard(
                        context->callback_context, &share);
                }
                break;
            case JOB_TYPE_SV2_EXTENDED:
                if (callbacks->submit_sv2_extended != NULL) {
                    callbacks->submit_sv2_extended(
                        context->callback_context, &share);
                }
                break;
        }
    }

    // A socket callback can yield while a clean job or reconnect retires this
    // result. Expected stale work must not be credited as new runtime proof.
    if (!asic_job_store_contains(context->job_store, result->work_handle) ||
        (context->work_is_current != NULL &&
         !context->work_is_current(context->callback_context, share.work_generation))) {
        return ASIC_RESULT_STALE_WORK;
    }
    if (callbacks->account_share != NULL) {
        callbacks->account_share(context->callback_context, &share);
    }

    return ASIC_RESULT_ACCOUNTED;
}
