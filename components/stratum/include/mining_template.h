#ifndef MINING_TEMPLATE_H_
#define MINING_TEMPLATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "mining.h"
#include "miner_job.h"
#include "stratum_api.h"

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

bool mining_template_build_sv1(const mining_notify *notification,
                               const char *extranonce_prefix,
                               uint32_t extranonce2_len,
                               uint64_t extranonce2_counter,
                               uint32_t version_mask, double difficulty,
                               mining_template_t *template);

bool mining_template_build_miner_job(const miner_job_t *job,
                                      uint64_t extranonce2_counter,
                                      uint32_t version,
                                      mining_template_t *template);

#endif // MINING_TEMPLATE_H_
