#ifndef BZM_FREQUENCY_QUALIFICATION_H
#define BZM_FREQUENCY_QUALIFICATION_H

#include <stdbool.h>
#include <stdint.h>

#include "bzm_driver.h"

typedef enum
{
    BZM_FREQUENCY_DOMAIN_INSUFFICIENT = 0,
    BZM_FREQUENCY_DOMAIN_PASS,
    BZM_FREQUENCY_DOMAIN_FAIL,
    BZM_FREQUENCY_DOMAIN_COUNTER_ROLLBACK,
} bzm_frequency_domain_verdict_t;

typedef struct
{
    uint8_t lead_zeros;
    uint32_t minimum_observed_results;
    float minimum_expected_pass_rate;
    float minimum_correct_result_ratio;
} bzm_frequency_qualification_config_t;

typedef struct
{
    bzm_frequency_domain_verdict_t verdict;
    uint64_t valid_results;
    uint64_t rejected_results;
    float expected_results;
    float expected_pass_rate;
    float correct_result_ratio;
} bzm_frequency_domain_result_t;

typedef struct
{
    bzm_frequency_domain_result_t
        domain[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT];
    uint8_t passed_domains;
    uint8_t failed_domains;
    uint8_t insufficient_domains;
    bool counter_rollback;
} bzm_frequency_qualification_result_t;

uint32_t bzm_frequency_qualification_window_ms(float frequency_mhz);
bool bzm_frequency_qualification_evaluate(
    const bzm_frequency_domain_stats_t *baseline,
    const bzm_frequency_domain_stats_t *current,
    const float frequency_mhz[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT],
    uint32_t elapsed_ms,
    const bzm_frequency_qualification_config_t *config,
    bzm_frequency_qualification_result_t *result);

#endif // BZM_FREQUENCY_QUALIFICATION_H
