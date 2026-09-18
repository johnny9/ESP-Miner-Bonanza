#ifndef STRATUM_SUBMISSION_H
#define STRATUM_SUBMISSION_H

#include "asic_result_handler.h"

typedef struct GlobalState GlobalState;

#define STRATUM_SHARE_QUEUE_LENGTH 32U

bool stratum_submission_init(GlobalState *state);
void stratum_submission_task(void *context);
/* Copies a validated share without waiting for queue space or network I/O.
 * Full queues drop the new network submission; local accounting continues. */
int stratum_queue_share(GlobalState *state, const asic_share_submission_t *share);

#endif
