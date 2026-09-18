#include "stratum_task.h"

void stratum_invalidate_work(GlobalState *state)
{
    pthread_mutex_lock(&state->transport_mutex);
    if (++state->stratum_work_generation == 0)
        ++state->stratum_work_generation;
    pthread_mutex_unlock(&state->transport_mutex);
}

bool stratum_work_is_current(GlobalState *state, uint64_t generation)
{
    /* Result validation and job creation must never wait behind a socket
     * write. Writers still serialize invalidation with the final send. */
    return generation == atomic_load_explicit(&state->stratum_work_generation,
                                              memory_order_acquire);
}

void stratum_publish_job(GlobalState *state, miner_job_t *job, uint8_t slot)
{
    pthread_mutex_lock(&state->transport_mutex);
    if (job->clean_jobs && ++state->stratum_work_generation == 0)
        ++state->stratum_work_generation;
    job->work_generation = state->stratum_work_generation;
    pthread_mutex_unlock(&state->transport_mutex);
    if (state->create_jobs_task_handle)
        xTaskNotify(state->create_jobs_task_handle, slot, eSetValueWithOverwrite);
}
