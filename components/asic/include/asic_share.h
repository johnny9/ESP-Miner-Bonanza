#ifndef ASIC_SHARE_H
#define ASIC_SHARE_H

#include "asic_result_handler.h"

/* Owned, relocatable queue entry. The submission's borrowed pointers are
 * cleared on capture and rebound only in a consumer-local view. Protocol
 * writers obtain the username from the validated connection, not the pool
 * settings that may have changed since this result was found. */
typedef struct {
    asic_share_submission_t submission;
    asic_result_t result;
    char job_id[ASIC_JOB_ID_LEN];
    char extranonce2[ASIC_JOB_EXTRANONCE2_HEX_SIZE];
} asic_share_snapshot_t;

bool asic_share_snapshot_capture(asic_share_snapshot_t *snapshot,
                                 const asic_share_submission_t *share);
asic_share_submission_t asic_share_snapshot_view(const asic_share_snapshot_t *snapshot);

#endif
