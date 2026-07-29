#include "bzm_frequency_qualification.h"
#include "unity.h"

static const bzm_frequency_qualification_config_t CONFIG = {
    .lead_zeros = 36,
    .minimum_observed_results = 8,
    .minimum_expected_pass_rate = 0.50f,
    .minimum_correct_result_ratio = 0.90f,
};

static void fill_frequency(
    float frequency[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT],
    float mhz)
{
    for (size_t asic = 0; asic < BZM_MAX_ASIC_COUNT; ++asic) {
        for (size_t domain = 0; domain < BZM_ENGINE_STACK_COUNT; ++domain) {
            frequency[asic][domain] = mhz;
        }
    }
}

TEST_CASE("BZM qualification uses the bzmd full-nonce dwell",
          "[asic][bzm][frequency][qualification]")
{
    TEST_ASSERT_EQUAL_UINT32(
        16912, bzm_frequency_qualification_window_ms(800.0f));
    TEST_ASSERT_EQUAL_UINT32(
        13530, bzm_frequency_qualification_window_ms(1000.0f));
    TEST_ASSERT_EQUAL_UINT32(
        10022, bzm_frequency_qualification_window_ms(1350.0f));
    TEST_ASSERT_EQUAL_UINT32(
        9020, bzm_frequency_qualification_window_ms(1500.0f));
    TEST_ASSERT_EQUAL_UINT32(
        0, bzm_frequency_qualification_window_ms(0.0f));
}

TEST_CASE("BZM qualification classifies every ASIC PLL domain",
          "[asic][bzm][frequency][qualification]")
{
    bzm_frequency_domain_stats_t baseline = {0};
    bzm_frequency_domain_stats_t current = {0};
    float frequency[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT];
    fill_frequency(frequency, 1000.0f);

    for (size_t asic = 0; asic < BZM_MAX_ASIC_COUNT; ++asic) {
        for (size_t domain = 0; domain < BZM_ENGINE_STACK_COUNT; ++domain) {
            current.valid[asic][domain] = 24;
            current.rejected[asic][domain] = 2;
        }
    }
    current.valid[2][1] = 3;
    current.rejected[2][1] = 23;

    bzm_frequency_qualification_result_t result;
    TEST_ASSERT_TRUE(bzm_frequency_qualification_evaluate(
        &baseline, &current, frequency, 13530, &CONFIG, &result));
    TEST_ASSERT_EQUAL_UINT8(7, result.passed_domains);
    TEST_ASSERT_EQUAL_UINT8(1, result.failed_domains);
    TEST_ASSERT_EQUAL(BZM_FREQUENCY_DOMAIN_FAIL,
                      result.domain[2][1].verdict);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 3.0f / 26.0f,
        result.domain[2][1].correct_result_ratio);
}

TEST_CASE("BZM qualification rejects counter rollback and sparse data",
          "[asic][bzm][frequency][qualification]")
{
    bzm_frequency_domain_stats_t baseline = {0};
    bzm_frequency_domain_stats_t current = {0};
    float frequency[BZM_MAX_ASIC_COUNT][BZM_ENGINE_STACK_COUNT];
    fill_frequency(frequency, 800.0f);

    for (size_t asic = 0; asic < BZM_MAX_ASIC_COUNT; ++asic) {
        for (size_t domain = 0; domain < BZM_ENGINE_STACK_COUNT; ++domain) {
            baseline.valid[asic][domain] = 10;
            current.valid[asic][domain] = 12;
        }
    }

    bzm_frequency_qualification_result_t result;
    TEST_ASSERT_TRUE(bzm_frequency_qualification_evaluate(
        &baseline, &current, frequency, 16912, &CONFIG, &result));
    TEST_ASSERT_EQUAL_UINT8(8, result.insufficient_domains);

    current.valid[0][0] = 9;
    TEST_ASSERT_FALSE(bzm_frequency_qualification_evaluate(
        &baseline, &current, frequency, 16912, &CONFIG, &result));
    TEST_ASSERT_TRUE(result.counter_rollback);
    TEST_ASSERT_EQUAL(BZM_FREQUENCY_DOMAIN_COUNTER_ROLLBACK,
                      result.domain[0][0].verdict);
}
