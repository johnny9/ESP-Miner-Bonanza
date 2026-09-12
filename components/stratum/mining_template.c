#include "mining_template.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

static bool duplicate_metadata(mining_template_t *template,
                               const char *job_id, const char *extranonce2)
{
    template->share.job_id = strdup(job_id ? job_id : "");
    template->share.extranonce2 = strdup(extranonce2 ? extranonce2 : "");
    if (template->share.job_id == NULL ||
        template->share.extranonce2 == NULL) {
        mining_template_free(template);
        return false;
    }
    return true;
}

bool mining_template_build_sv1(const mining_notify *notification,
                               const char *extranonce_prefix,
                               uint32_t extranonce2_len,
                               uint64_t extranonce2_counter,
                               uint32_t version_mask, double difficulty,
                               mining_template_t *template)
{
    if (template == NULL) return false;
    memset(template, 0, sizeof(*template));
    if (notification == NULL || extranonce_prefix == NULL ||
        extranonce2_len > MINING_MAX_EXTRANONCE2_SIZE) {
        return false;
    }

    char extranonce2[MINING_MAX_EXTRANONCE2_SIZE * 2 + 1];
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
    template->target = notification->target;
    template->clean_jobs = notification->clean_jobs;
    template->share.protocol = MINING_PROTOCOL_SV1;
    template->share.pool_difficulty = difficulty;
    reverse_32bit_words(merkle_root, template->merkle_root);

    uint8_t prev_block_hash[32];
    if (hex2bin(notification->prev_block_hash, prev_block_hash,
                sizeof(prev_block_hash)) != sizeof(prev_block_hash)) {
        return false;
    }
    reverse_endianness_per_word(prev_block_hash);
    reverse_32bit_words(prev_block_hash, template->prev_block_hash);
    return duplicate_metadata(template, notification->job_id, extranonce2);
}

// Materialize the upstream pool slot into owned work independent of slot reuse.
bool mining_template_build_miner_job(const miner_job_t *job,
                                      uint64_t extranonce2_counter,
                                      uint32_t version,
                                      mining_template_t *template)
{
    if (template == NULL) return false;
    memset(template, 0, sizeof(*template));
    if (job == NULL || job->type < JOB_TYPE_V1 || job->type > JOB_TYPE_SV2_EXTENDED ||
        job->extranonce2_len > MINING_MAX_EXTRANONCE2_SIZE ||
        job->extranonce1_len > sizeof(job->extranonce1) ||
        job->merkle_path_count > MAX_MERKLE_BRANCHES ||
        (job->coinbase_prefix_len != 0 && job->coinbase_prefix == NULL) ||
        (job->coinbase_suffix_len != 0 && job->coinbase_suffix == NULL)) {
        return false;
    }

    uint8_t merkle_root[32];
    char extranonce2[MINING_MAX_EXTRANONCE2_SIZE * 2 + 1] = "";
    if (job->type == JOB_TYPE_SV2_STANDARD) {
        memcpy(merkle_root, job->merkle_root, sizeof(merkle_root));
    } else {
        size_t len = job->extranonce2_len;
        size_t copy_len = len < sizeof(extranonce2_counter) ? len : sizeof(extranonce2_counter);
        memcpy(template->share.extranonce2_bin, &extranonce2_counter, copy_len);
        template->share.extranonce2_len = len;
        bin2hex(template->share.extranonce2_bin, len, extranonce2, sizeof(extranonce2));
        uint8_t coinbase_hash[32];
        if (!calculate_coinbase_tx_hash_bin(job->coinbase_prefix, job->coinbase_prefix_len,
                                       job->extranonce1, job->extranonce1_len,
                                       template->share.extranonce2_bin, len,
                                       job->coinbase_suffix, job->coinbase_suffix_len,
                                       coinbase_hash)) return false;
        calculate_merkle_root_hash(coinbase_hash, job->merkle_path,
                                   job->merkle_path_count, merkle_root);
    }
    template->version = version;
    template->version_mask = job->version_mask;
    template->ntime = job->ntime;
    template->target = job->nbits;
    template->clean_jobs = job->clean_jobs;
    template->share.protocol = (mining_protocol_t)job->type;
    template->share.pool_id = job->pool_id;
    template->share.pool_difficulty = job->pool_diff;
    template->share.numeric_job_id = (uint32_t)strtoul(job->job_id, NULL, 10);
    reverse_32bit_words(job->prev_hash, template->prev_block_hash);
    reverse_32bit_words(merkle_root, template->merkle_root);
    return duplicate_metadata(template, job->job_id, extranonce2);
}
