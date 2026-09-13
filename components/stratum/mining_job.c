#include "mining_job.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

static bool copy_metadata(asic_job_t *job, const char *job_id,
                           const char *extranonce2)
{
    if (job_id == NULL || extranonce2 == NULL ||
        strnlen(job_id, sizeof(job->job_id)) == sizeof(job->job_id) ||
        strnlen(extranonce2, sizeof(job->extranonce2)) == sizeof(job->extranonce2)) {
        return false;
    }
    strcpy(job->job_id, job_id);
    strcpy(job->extranonce2, extranonce2);
    return true;
}

bool mining_build_asic_job_sv1(const mining_notify *notification,
                               const char *extranonce_prefix,
                               uint32_t extranonce2_len,
                               uint64_t extranonce2_counter,
                               uint32_t version_mask, double difficulty,
                               asic_job_t *destination)
{
    if (destination == NULL) return false;
    asic_job_t built = {0};
    asic_job_t *template = &built;
    if (notification == NULL || extranonce_prefix == NULL ||
        extranonce2_len > ASIC_JOB_EXTRANONCE2_SIZE) {
        return false;
    }

    char extranonce2[ASIC_JOB_EXTRANONCE2_SIZE * 2 + 1];
    extranonce_2_generate(extranonce2_counter, extranonce2_len, extranonce2);

    uint8_t coinbase_hash[32];
    uint8_t merkle_root[32];
    calculate_coinbase_tx_hash(notification->coinbase_1,
                               notification->coinbase_2,
                               extranonce_prefix, extranonce2,
                               coinbase_hash);
    calculate_merkle_root_hash(
        coinbase_hash,
        (const uint8_t (*)[32])notification->merkle_branches,
        notification->n_merkle_branches, merkle_root);

    template->version = notification->version;
    template->version_mask = version_mask;
    template->ntime = notification->ntime;
    template->nbits = notification->target;
    template->clean_jobs = notification->clean_jobs;
    template->source_type = JOB_TYPE_V1;
    template->job_version = notification->version;
    template->pool_diff = difficulty;
    memcpy(template->merkle_root, merkle_root, 32);

    uint8_t prev_block_hash[32];
    if (hex2bin(notification->prev_block_hash, prev_block_hash,
                sizeof(prev_block_hash)) != sizeof(prev_block_hash)) {
        return false;
    }
    reverse_endianness_per_word(prev_block_hash);
    memcpy(template->prev_hash, prev_block_hash, 32);
    if (!copy_metadata(template, notification->job_id, extranonce2)) return false;
    *destination = built;
    return true;
}

// Materialize the upstream pool slot into owned work independent of slot reuse.
bool mining_build_asic_job(const miner_job_t *job,
                                      uint64_t extranonce2_counter,
                                      uint32_t version,
                                      asic_job_t *destination)
{
    if (destination == NULL) return false;
    asic_job_t built = {0};
    asic_job_t *template = &built;
    if (job == NULL || job->type < JOB_TYPE_V1 || job->type > JOB_TYPE_SV2_EXTENDED ||
        memchr(job->job_id, 0, sizeof(job->job_id)) == NULL ||
        job->extranonce2_len > ASIC_JOB_EXTRANONCE2_SIZE ||
        job->extranonce1_len > sizeof(job->extranonce1) ||
        job->merkle_path_count > MAX_MERKLE_BRANCHES ||
        (job->coinbase_prefix_len != 0 && job->coinbase_prefix == NULL) ||
        (job->coinbase_suffix_len != 0 && job->coinbase_suffix == NULL)) {
        return false;
    }

    uint8_t merkle_root[32];
    char extranonce2[ASIC_JOB_EXTRANONCE2_SIZE * 2 + 1] = "";
    if (job->type == JOB_TYPE_SV2_STANDARD) {
        memcpy(merkle_root, job->merkle_root, sizeof(merkle_root));
    } else {
        size_t len = job->extranonce2_len;
        size_t copy_len = len < sizeof(extranonce2_counter) ? len : sizeof(extranonce2_counter);
        uint8_t extranonce2_bin[ASIC_JOB_EXTRANONCE2_SIZE] = {0};
        for (size_t i = 0; i < copy_len; ++i)
            extranonce2_bin[i] = (uint8_t)(extranonce2_counter >> (8 * i));
        bin2hex(extranonce2_bin, len, extranonce2, sizeof(extranonce2));
        uint8_t coinbase_hash[32];
        if (!calculate_coinbase_tx_hash_bin(job->coinbase_prefix, job->coinbase_prefix_len,
                                       job->extranonce1, job->extranonce1_len,
                                       extranonce2_bin, len,
                                       job->coinbase_suffix, job->coinbase_suffix_len,
                                       coinbase_hash)) return false;
        calculate_merkle_root_hash(coinbase_hash, job->merkle_path,
                                   job->merkle_path_count, merkle_root);
    }
    template->version = version != 0 ? version : job->version;
    template->version_mask = job->version_mask;
    template->ntime = job->ntime;
    template->nbits = job->nbits;
    template->clean_jobs = job->clean_jobs;
    template->source_type = (mining_job_source_t)job->type;
    template->pool_id = job->pool_id;
    template->work_generation = job->work_generation;
    template->job_version = job->version;
    template->pool_diff = job->pool_diff;
    memcpy(template->prev_hash, job->prev_hash, 32);
    memcpy(template->merkle_root, merkle_root, 32);
    if (!copy_metadata(template, job->job_id, extranonce2)) return false;
    *destination = built;
    return true;
}
