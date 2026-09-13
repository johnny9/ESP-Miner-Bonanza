#include "miner_job.h"
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_psram.h"

static miner_job_t s_job_pool[MINER_JOB_POOL_SIZE];

bool miner_job_ensure_buffers(miner_job_t *job)
{
    if (!job) return false;
#if defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
    bool has_psram = esp_psram_is_initialized();
#else
    bool has_psram = false;
#endif
    if (!job->coinbase_prefix) {
        job->coinbase_prefix_capacity = MAX_COINBASE_PREFIX_LEN;
        job->coinbase_prefix = calloc(1, job->coinbase_prefix_capacity);
    }
    if (!job->coinbase_suffix) {
        // No-PSRAM test/recovery environments have smaller buffers. Parsers
        // must honor the actual capacity, not the protocol's 64 KiB limit.
        job->coinbase_suffix_capacity = has_psram
            ? MAX_COINBASE_SUFFIX_LEN : 2048;
        job->coinbase_suffix = has_psram
            ? heap_caps_calloc(1, job->coinbase_suffix_capacity, MALLOC_CAP_SPIRAM)
            : calloc(1, job->coinbase_suffix_capacity);
    }
    return job->coinbase_prefix && job->coinbase_suffix;
}

void miner_job_reset(miner_job_t *job)
{
    uint8_t *prefix = job->coinbase_prefix;
    uint8_t *suffix = job->coinbase_suffix;
    size_t prefix_capacity = job->coinbase_prefix_capacity;
    size_t suffix_capacity = job->coinbase_suffix_capacity;
    memset(job, 0, sizeof(*job));
    job->coinbase_prefix = prefix;
    job->coinbase_suffix = suffix;
    job->coinbase_prefix_capacity = prefix_capacity;
    job->coinbase_suffix_capacity = suffix_capacity;
}

void miner_job_pool_init(void)
{
    for (size_t i = 0; i < MINER_JOB_POOL_SIZE; i++) {
        miner_job_ensure_buffers(&s_job_pool[i]);
        miner_job_reset(&s_job_pool[i]);
    }
}

miner_job_t *miner_job_get_slot(size_t index)
{
    miner_job_t *job = &s_job_pool[index % MINER_JOB_POOL_SIZE];
    miner_job_ensure_buffers(job);
    return job;
}
