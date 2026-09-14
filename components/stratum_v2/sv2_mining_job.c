#include "sv2_mining_job.h"

#include <inttypes.h>
#include <stdio.h>
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

bool mining_build_asic_job_sv2_standard(const sv2_job_t *source,
                                        uint32_t version_mask,
                                        double difficulty,
                                        asic_job_t *destination)
{
    if (destination == NULL) return false;
    asic_job_t built = {0};
    asic_job_t *template = &built;
    if (source == NULL) return false;

    template->version = source->version;
    template->version_mask = version_mask;
    template->nbits = source->nbits;
    template->ntime = source->ntime;
    template->source_type = JOB_TYPE_SV2_STANDARD;
    template->pool_diff = difficulty;
    memcpy(template->merkle_root, source->merkle_root, 32);
    memcpy(template->prev_hash, source->prev_hash, 32);

    char job_id[16];
    snprintf(job_id, sizeof(job_id), "%" PRIu32, source->job_id);
    if (!copy_metadata(template, job_id, "")) return false;
    *destination = built;
    return true;
}

bool mining_build_asic_job_sv2_extended(const sv2_ext_job_t *source,
                                        const sv2_conn_t *connection,
                                        uint64_t extranonce2_counter,
                                        uint32_t version_mask,
                                        double difficulty,
                                        asic_job_t *destination)
{
    if (destination == NULL) return false;
    asic_job_t built = {0};
    asic_job_t *template = &built;
    if (source == NULL || connection == NULL ||
        connection->extranonce_prefix_len >
            sizeof(connection->extranonce_prefix) ||
        connection->extranonce_size > ASIC_JOB_EXTRANONCE2_SIZE ||
        source->merkle_path_count > SV2_MAX_MERKLE_BRANCHES) {
        return false;
    }

    uint8_t extranonce2[ASIC_JOB_EXTRANONCE2_SIZE] = {0};
    for (int i = connection->extranonce_size - 1;
         i >= 0 && extranonce2_counter > 0; --i) {
        extranonce2[i] = (uint8_t)(extranonce2_counter & 0xff);
        extranonce2_counter >>= 8;
    }

    uint8_t coinbase_hash[32];
    uint8_t merkle_root[32];
    if (!calculate_coinbase_tx_hash_bin(
        source->coinbase_prefix, source->coinbase_prefix_len,
        connection->extranonce_prefix, connection->extranonce_prefix_len,
        extranonce2, connection->extranonce_size,
        source->coinbase_suffix, source->coinbase_suffix_len,
        coinbase_hash)) return false;
    calculate_merkle_root_hash(coinbase_hash, source->merkle_path,
                               source->merkle_path_count, merkle_root);

    template->version = source->version;
    template->version_mask = version_mask;
    template->nbits = source->nbits;
    template->ntime = source->ntime;
    template->source_type = JOB_TYPE_SV2_EXTENDED;
    template->pool_diff = difficulty;
    memcpy(template->merkle_root, merkle_root, 32);
    memcpy(template->prev_hash, source->prev_hash, 32);

    char job_id[16];
    char extranonce2_hex[ASIC_JOB_EXTRANONCE2_SIZE * 2 + 1];
    snprintf(job_id, sizeof(job_id), "%" PRIu32, source->job_id);
    bin2hex(extranonce2, connection->extranonce_size, extranonce2_hex,
            sizeof(extranonce2_hex));
    if (!copy_metadata(template, job_id, extranonce2_hex)) return false;
    *destination = built;
    return true;
}
