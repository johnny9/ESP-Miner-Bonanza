#include "bzm_frequency_qualification.h"

#include <math.h>
#include <string.h>

static bool config_is_valid(
    const bzm_frequency_qualification_config_t *config)
{
    return config != NULL && config->lead_zeros >= 32U &&
           config->lead_zeros < 63U &&
           config->minimum_observed_results != 0U &&
           isfinite(config->minimum_expected_pass_rate) &&
           config->minimum_expected_pass_rate > 0.0f &&
           config->minimum_expected_pass_rate <= 1.0f &&
           isfinite(config->minimum_correct_result_ratio) &&
           config->minimum_correct_result_ratio > 0.0f &&
           config->minimum_correct_result_ratio <= 1.0f;
}

uint32_t bzm_frequency_qualification_window_ms(float frequency_mhz)
{
    if (!isfinite(frequency_mhz) || frequency_mhz <= 0.0f) return 0;

    const double nonce_count = 4294967296.0;
    double seconds =
        1.05 * nonce_count / ((double)frequency_mhz * 1000000.0 / 3.0);
    double milliseconds = ceil(seconds * 1000.0);
    if (milliseconds <= 0.0 || milliseconds > (double)UINT32_MAX) return 0;
    return (uint32_t)milliseconds;
}

bool bzm_frequency_qualification_evaluate(
    const bzm_frequency_domain_stats_t *baseline,
    const bzm_frequency_domain_stats_t *current,
    const float frequency_mhz[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT],
    uint32_t elapsed_ms,
    const bzm_frequency_qualification_config_t *config,
    bzm_frequency_qualification_result_t *result)
{
    if (baseline == NULL || current == NULL || frequency_mhz == NULL ||
        elapsed_ms == 0U || !config_is_valid(config) || result == NULL) {
        return false;
    }

    memset(result, 0, sizeof(*result));
    const double hashes_per_report = ldexp(1.0, config->lead_zeros);
    const double elapsed_seconds = (double)elapsed_ms / 1000.0;

    for (size_t asic = 0; asic < BZM_MAX_ASIC_COUNT; ++asic) {
        for (size_t domain = 0; domain < BZM_ENGINE_STACK_COUNT; ++domain) {
            bzm_frequency_domain_result_t *domain_result =
                &result->domain[asic][domain];
            if (current->valid[asic][domain] <
                    baseline->valid[asic][domain] ||
                current->rejected[asic][domain] <
                    baseline->rejected[asic][domain]) {
                domain_result->verdict =
                    BZM_FREQUENCY_DOMAIN_COUNTER_ROLLBACK;
                result->counter_rollback = true;
                continue;
            }

            float mhz = frequency_mhz[asic][domain];
            if (!isfinite(mhz) || mhz <= 0.0f) return false;

            domain_result->valid_results =
                current->valid[asic][domain] -
                baseline->valid[asic][domain];
            domain_result->rejected_results =
                current->rejected[asic][domain] -
                baseline->rejected[asic][domain];
            uint64_t observed =
                domain_result->valid_results +
                domain_result->rejected_results;

            /* One PLL serves one 118-engine stack; each engine has four TCEs. */
            double expected_hashes =
                (double)BZM_TOPOLOGY_STACK_ENGINE_COUNT * 4.0 / 3.0 *
                (double)mhz * 1000000.0 * elapsed_seconds;
            domain_result->expected_results =
                (float)(expected_hashes / hashes_per_report);
            domain_result->expected_pass_rate =
                domain_result->expected_results > 0.0f
                    ? (float)domain_result->valid_results /
                          domain_result->expected_results
                    : 0.0f;
            domain_result->correct_result_ratio =
                observed != 0U
                    ? (float)domain_result->valid_results / (float)observed
                    : 0.0f;

            if (observed < config->minimum_observed_results) {
                domain_result->verdict =
                    BZM_FREQUENCY_DOMAIN_INSUFFICIENT;
                ++result->insufficient_domains;
            } else if (
                domain_result->expected_pass_rate >=
                    config->minimum_expected_pass_rate &&
                domain_result->correct_result_ratio >=
                    config->minimum_correct_result_ratio) {
                domain_result->verdict = BZM_FREQUENCY_DOMAIN_PASS;
                ++result->passed_domains;
            } else {
                domain_result->verdict = BZM_FREQUENCY_DOMAIN_FAIL;
                ++result->failed_domains;
            }
        }
    }
    return !result->counter_rollback;
}
