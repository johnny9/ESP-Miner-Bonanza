#ifndef ASIC_JOB_STORE_H
#define ASIC_JOB_STORE_H

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

#include "asic_result.h"
#include "asic_job.h"

/* BZM keeps one independently generated job active for each of its 236
 * logical engines. Keep a full 8-bit hardware-handle slot space so every
 * current assignment remains addressable while the scheduler rotates. */
#define ASIC_JOB_STORE_CAPACITY 256

/* Bonanza's internal assignment bookkeeping, never part of asic_job_t or
 * the common job encoder. Copied with a retained assignment under the lock. */
typedef struct {
    uint64_t work_generation;
    uint32_t job_version;
    bool clean_jobs;
    bool pool_work;
} asic_job_context_t;

typedef struct {
    bool valid;
    asic_work_handle_t handle;
    asic_job_t template;
    asic_job_context_t context;
} asic_job_store_entry_t;

typedef struct {
    pthread_mutex_t lock;
    pthread_mutex_t submission_lock;
    const asic_job_t *submission_job;
    asic_job_context_t submission_context;
    asic_job_store_entry_t *entries;
    uint16_t capacity;
    uint16_t next_slot;
    uint64_t next_generation;
} asic_job_store_t;

bool asic_job_store_init(asic_job_store_t *store);
bool asic_job_store_init_with_caps(asic_job_store_t *store,
                                   uint32_t memory_caps);
void asic_job_store_destroy(asic_job_store_t *store);

/* Serialize the producer's borrowed-job scope with its protocol provenance.
 * Every successful begin must have an end, including cancelled submission.
 * Invalidation uses only lock and can interrupt a blocked submission. */
void asic_job_store_begin_submission(asic_job_store_t *store,
                                     const asic_job_t *job,
                                     const asic_job_context_t *context);
void asic_job_store_end_submission(asic_job_store_t *store);
bool asic_job_store_submission_context(asic_job_store_t *store,
                                       const asic_job_t *job,
                                       asic_job_context_t *context);

// Compatibility mode for ASICs whose hardware result contains only a slot id.
bool asic_job_store_store_slot(asic_job_store_t *store, uint8_t slot,
                               const asic_job_t *template,
                               asic_work_handle_t *handle);

// Generation-bearing handles reject stale results after reuse or invalidation.
bool asic_job_store_store_generated(asic_job_store_t *store,
                                    const asic_job_t *template,
                                    asic_work_handle_t *handle);

bool asic_job_store_snapshot(asic_job_store_t *store,
                             asic_work_handle_t handle,
                             asic_job_t *snapshot);
bool asic_job_store_snapshot_with_context(asic_job_store_t *store,
                                          asic_work_handle_t handle,
                                          asic_job_t *snapshot,
                                          asic_job_context_t *context);
// Read-only identity check used by drivers before emitting a delayed result.
bool asic_job_store_contains(asic_job_store_t *store,
                             asic_work_handle_t handle);
bool asic_job_store_release(asic_job_store_t *store,
                            asic_work_handle_t handle);
void asic_job_store_invalidate_all(asic_job_store_t *store);

#endif // ASIC_JOB_STORE_H
