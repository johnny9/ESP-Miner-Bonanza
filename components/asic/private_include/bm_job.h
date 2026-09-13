#ifndef BM_JOB_H_
#define BM_JOB_H_

#include <stdint.h>

// Bitmain-family packet material. Pool metadata remains in asic_job_t
// and is never copied into this private representation.
typedef struct bm_job {
    uint32_t version;
    uint32_t version_mask;
    uint8_t prev_block_hash[32];
    uint8_t merkle_root[32];
    uint32_t ntime;
    uint32_t target;
    uint32_t starting_nonce;
    uint8_t num_midstates;
    union {
        struct {
            uint8_t midstate[32], midstate1[32], midstate2[32], midstate3[32];
        };
        uint8_t midstates[4][32];
    };
} bm_job;

#endif // BM_JOB_H_
