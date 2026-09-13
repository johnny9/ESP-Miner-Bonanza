#ifndef MINING_TEST_BINDINGS_H
#define MINING_TEST_BINDINGS_H

#include <stdlib.h>
#include <string.h>
#include "mining_allocator_fault_injector.h"

/* Give the private test instance unique symbols so unrelated tests and system
 * tasks cannot consume an injected allocation failure. */
#define calculate_coinbase_tx_hash mining_test_calculate_coinbase_tx_hash
#define calculate_coinbase_tx_hash_bin mining_test_calculate_coinbase_tx_hash_bin
#define calculate_merkle_root_hash mining_test_calculate_merkle_root_hash
#define hash_to_pdiff mining_test_hash_to_pdiff
#define mining_test_nonce_value mining_test_nonce_value_private
#define increment_bitmask mining_test_increment_bitmask
#define extranonce_2_generate mining_test_extranonce_2_generate
#define mining_build_asic_job_sv1 mining_test_template_build_sv1
#define mining_build_asic_job mining_test_template_build_miner_job

#define malloc(size) mining_allocator_fault_injector_malloc(size)
#define strdup(text) mining_allocator_fault_injector_strdup(text)

#endif
