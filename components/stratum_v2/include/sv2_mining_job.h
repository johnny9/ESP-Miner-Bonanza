#ifndef SV2_MINING_JOB_H_
#define SV2_MINING_JOB_H_

#include <stdbool.h>
#include <stdint.h>

#include "mining.h"
#include "sv2_protocol.h"

// Legacy template inputs retained for offline work/result regression vectors.
typedef struct {
    uint32_t job_id;
    uint32_t version;
    uint8_t merkle_root[32]; // Internal byte order (as received from SV2)
    uint8_t prev_hash[32];   // Internal byte order (as received from SV2)
    uint32_t ntime;
    uint32_t nbits;
    bool clean_jobs;
} sv2_job_t;

typedef struct {
    uint32_t job_id;
    uint32_t version;
    bool     version_rolling_allowed;
    uint8_t  prev_hash[32];
    uint32_t ntime;
    uint32_t nbits;
    bool     clean_jobs;
    uint8_t  merkle_path[SV2_MAX_MERKLE_BRANCHES][32];
    uint8_t  merkle_path_count;
    uint8_t *coinbase_prefix;     // heap
    uint16_t coinbase_prefix_len;
    uint8_t *coinbase_suffix;     // heap
    uint16_t coinbase_suffix_len;
} sv2_ext_job_t;

bool mining_build_asic_job_sv2_standard(const sv2_job_t *source,
                                        uint32_t version_mask,
                                        double difficulty,
                                        asic_job_t *template);

bool mining_build_asic_job_sv2_extended(const sv2_ext_job_t *source,
                                        const sv2_conn_t *connection,
                                        uint64_t extranonce2_counter,
                                        uint32_t version_mask,
                                        double difficulty,
                                        asic_job_t *template);

#endif // SV2_MINING_JOB_H_
