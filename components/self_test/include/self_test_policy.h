#ifndef SELF_TEST_POLICY_H_
#define SELF_TEST_POLICY_H_
#include <stddef.h>
#include <stdbool.h>
#include "asic_job.h"

enum { SELF_TEST_MIN_DOMAIN_SAMPLES = 3, SELF_TEST_MIN_NONCES = 2 };
bool self_test_deadline_expired(uint64_t start_us, uint64_t now_us, uint64_t duration_us);

typedef struct {
    float hashrate_sum;
    uint32_t sample_count, rejected_sample_count;
    uint64_t last_sample_time_us, generation;
} self_test_domain_average_t;
typedef struct {
    float hashrate_ghs;
    uint64_t acquired_at_us, generation;
    bool valid;
} self_test_domain_sample_t;
typedef enum {
    SELF_TEST_DOMAIN_OK, SELF_TEST_DOMAIN_FAIL, SELF_TEST_DOMAIN_UNRELIABLE,
} self_test_domain_status_t;

void self_test_domain_add(self_test_domain_average_t *average,
    self_test_domain_sample_t sample, float expected_ghs);
float self_test_domain_average(const self_test_domain_average_t *average);
self_test_domain_status_t self_test_domain_status(const self_test_domain_average_t *average,
    float expected_ghs);
bool self_test_in_range(float value, float target, float tolerance);
/* The first header matches the original self-test's fixed transaction and
 * extranonce zero. Work is local and never carries a pool destination. */
void self_test_build_job(asic_job_t *job);
#endif
