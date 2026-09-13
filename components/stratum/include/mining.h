#ifndef MINING_H_
#define MINING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "asic_job.h"
#include "miner_job.h"

/* Complete owned work; destination is unchanged on failure.
 * A zero version selects the source version, matching the upstream contract. */
bool mining_build_asic_job(const miner_job_t *source, uint64_t extranonce2,
                           uint32_t version, asic_job_t *destination);

void calculate_coinbase_tx_hash(const char *coinbase_1, const char *coinbase_2,
                                const char *extranonce, const char *extranonce_2, uint8_t dest[32]);

bool calculate_coinbase_tx_hash_bin(const uint8_t *prefix, size_t prefix_len,
                                    const uint8_t *extranonce_prefix, size_t ep_len,
                                    const uint8_t *extranonce_2, size_t e2_len,
                                    const uint8_t *suffix, size_t suffix_len,
                                    uint8_t dest[32]);

void calculate_merkle_root_hash(const uint8_t coinbase_tx_hash[32], const uint8_t merkle_branches[][32], const int num_merkle_branches, uint8_t dest[32]);

// Convert a 256-bit value (block hash or pool target, little-endian) to
// difficulty (pdiff = truediffone / value). Shared by SV1 (test_nonce_value)
// and SV2 (target). Returns a double to preserve fractional difficulty.
double hash_to_pdiff(const uint8_t hash[32]);

double mining_test_nonce_value(const asic_job_t *template,
                               uint32_t nonce, uint32_t final_ntime,
                               uint32_t final_version);

void extranonce_2_generate(uint64_t extranonce_2, uint32_t length, char *dest);

uint32_t increment_bitmask(const uint32_t value, const uint32_t mask);

#endif /* MINING_H_ */
