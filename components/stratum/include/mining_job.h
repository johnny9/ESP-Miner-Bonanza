#ifndef MINING_JOB_H_
#define MINING_JOB_H_

#include <stdbool.h>
#include <stdint.h>

#include "mining.h"
#include "miner_job.h"

// Legacy hexadecimal template input, retained for offline vectors.
typedef struct mining_notify
{
    char *job_id;
    char *prev_block_hash;
    char *coinbase_1;
    char *coinbase_2;
    uint8_t *merkle_branches;
    size_t n_merkle_branches;
    uint32_t version;
    uint32_t target;
    uint32_t ntime;
    bool clean_jobs;
} mining_notify;

bool mining_build_asic_job_sv1(const mining_notify *notification,
                               const char *extranonce_prefix,
                               uint32_t extranonce2_len,
                               uint64_t extranonce2_counter,
                               uint32_t version_mask, double difficulty,
                               asic_job_t *template);

#endif // MINING_JOB_H_
