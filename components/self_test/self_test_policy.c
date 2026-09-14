#include "self_test_policy.h"
#include <math.h>

bool self_test_deadline_expired(uint64_t start_us, uint64_t now_us, uint64_t duration_us)
{
    return now_us < start_us || now_us - start_us >= duration_us;
}

void self_test_domain_add(self_test_domain_average_t *average,
    self_test_domain_sample_t sample, float expected_ghs)
{
    if (!sample.valid || sample.acquired_at_us == 0) return;
    if (average->generation != sample.generation) {
        *average = (self_test_domain_average_t){.generation = sample.generation};
    }
    if (sample.acquired_at_us <= average->last_sample_time_us) return;
    bool first = average->last_sample_time_us == 0;
    average->last_sample_time_us = sample.acquired_at_us;
    if (first) return;
    if (!isfinite(sample.hashrate_ghs) || sample.hashrate_ghs < 0 || sample.hashrate_ghs > expected_ghs * 3) {
        if (average->rejected_sample_count != UINT32_MAX) average->rejected_sample_count++;
        return;
    }
    if (average->sample_count != UINT32_MAX) {
        average->hashrate_sum += sample.hashrate_ghs;
        average->sample_count++;
    }
}

float self_test_domain_average(const self_test_domain_average_t *average)
{
    return average->sample_count > 0 ? average->hashrate_sum / average->sample_count : 0;
}

self_test_domain_status_t self_test_domain_status(const self_test_domain_average_t *average,
    float expected_ghs)
{
    uint64_t total = (uint64_t)average->sample_count + average->rejected_sample_count;
    if (total > 0 && (double)average->rejected_sample_count / total >= 0.25)
        return SELF_TEST_DOMAIN_UNRELIABLE;
    float measured = self_test_domain_average(average);
    if (average->sample_count < SELF_TEST_MIN_DOMAIN_SAMPLES || measured < expected_ghs * (1.0f - 0.33f) || measured > expected_ghs * (1.0f + 0.33f))
        return SELF_TEST_DOMAIN_FAIL;
    return SELF_TEST_DOMAIN_OK;
}

bool self_test_in_range(float value, float target, float tolerance)
{
    return isfinite(value) && value >= target * (1 - tolerance) && value <= target * (1 + tolerance);
}
