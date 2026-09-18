#include "asic_share.h"

#include <string.h>

bool asic_share_snapshot_capture(asic_share_snapshot_t *snapshot,
                                 const asic_share_submission_t *share)
{
    if (!snapshot || !share || !share->result || !share->job_id || !share->extranonce2 ||
        share->protocol < JOB_TYPE_V1 || share->protocol > JOB_TYPE_SV2_EXTENDED ||
        share->extranonce2_len > ASIC_JOB_EXTRANONCE2_SIZE) return false;
    size_t job_length = strnlen(share->job_id, sizeof(snapshot->job_id));
    size_t extranonce_length = strnlen(share->extranonce2, sizeof(snapshot->extranonce2));
    if (job_length == sizeof(snapshot->job_id) ||
        extranonce_length == sizeof(snapshot->extranonce2)) return false;

    *snapshot = (asic_share_snapshot_t){.submission = *share, .result = *share->result};
    snapshot->submission.result = NULL;
    snapshot->submission.username = NULL;
    snapshot->submission.job_id = NULL;
    snapshot->submission.extranonce2 = NULL;
    memcpy(snapshot->job_id, share->job_id, job_length + 1);
    memcpy(snapshot->extranonce2, share->extranonce2, extranonce_length + 1);
    return true;
}

asic_share_submission_t asic_share_snapshot_view(const asic_share_snapshot_t *snapshot)
{
    asic_share_submission_t share = snapshot->submission;
    share.result = &snapshot->result;
    share.job_id = snapshot->job_id;
    share.extranonce2 = snapshot->extranonce2;
    return share;
}
