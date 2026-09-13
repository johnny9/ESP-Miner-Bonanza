#include "bm_job_builder.h"

#include <stddef.h>
#include <string.h>

#include "utils.h"
#include "mining.h"

bool bm_job_build_from_asic_job(const asic_job_t *template, bm_job *job)
{
    if (template == NULL || job == NULL) return false;

    memset(job, 0, sizeof(*job));
    job->version = template->version;
    job->version_mask = template->version_mask;
    job->ntime = template->ntime;
    job->target = template->nbits;
    job->starting_nonce = template->starting_nonce;
    reverse_32bit_words(template->prev_hash, job->prev_block_hash);
    reverse_32bit_words(template->merkle_root, job->merkle_root);

    uint8_t midstate_data[80];
    uint8_t midstate[32];
    uint32_t rolled_version = template->version;
    asic_job_header(template, template->starting_nonce, rolled_version, midstate_data);
    midstate_sha256_bin(midstate_data, 64, midstate);
    reverse_32bit_words(midstate, job->midstate);

    if (template->version_mask == 0) {
        job->num_midstates = 1;
        return true;
    }

    uint8_t *destinations[] = {
        job->midstate1, job->midstate2, job->midstate3,
    };
    for (size_t i = 0; i < 3; ++i) {
        rolled_version = increment_bitmask(rolled_version,
                                           template->version_mask);
        asic_job_header(template, template->starting_nonce, rolled_version, midstate_data);
        midstate_sha256_bin(midstate_data, 64, midstate);
        reverse_32bit_words(midstate, destinations[i]);
    }
    job->num_midstates = 4;
    return true;
}
