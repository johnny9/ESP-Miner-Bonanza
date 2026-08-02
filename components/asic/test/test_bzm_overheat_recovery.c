#include <math.h>
#include <string.h>

#include "bzm_overheat_recovery.h"
#include "unity.h"

TEST_CASE("BZM overheat recovery enforces upstream cooling gates",
          "[asic][bzm][overheat]")
{
    bzm_overheat_recovery_t recovery;
    TEST_ASSERT_TRUE(bzm_overheat_recovery_begin(
        &recovery, 1000, 3000, 1250.0f));

    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_OFF_SAFE,
        bzm_overheat_recovery_evaluate(
            &recovery, 31000, false, true, 50.0f, false, 0.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY,
        bzm_overheat_recovery_evaluate(
            &recovery, 31000, true, false, 0.0f, false, 0.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_MINIMUM,
        bzm_overheat_recovery_evaluate(
            &recovery, 30999, true, true, 50.0f, false, 0.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_VREG,
        bzm_overheat_recovery_evaluate(
            &recovery, 31000, true, true, 95.1f, false, 0.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_READY,
        bzm_overheat_recovery_evaluate(
            &recovery, 31000, true, true, 95.0f, false, 0.0f));
}

TEST_CASE("BZM overheat recovery restarts cooling proof for a hot ASIC",
          "[asic][bzm][overheat]")
{
    bzm_overheat_recovery_t recovery;
    TEST_ASSERT_TRUE(bzm_overheat_recovery_begin(
        &recovery, 500, 3000, 1250.0f));

    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_ASIC,
        bzm_overheat_recovery_evaluate(
            &recovery, 30500, true, true, 60.0f, true, 45.1f));
    TEST_ASSERT_EQUAL_UINT32(30500,
                             (uint32_t)recovery.cooling_since_ms);
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_MINIMUM,
        bzm_overheat_recovery_evaluate(
            &recovery, 60499, true, true, 60.0f, true, 45.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_READY,
        bzm_overheat_recovery_evaluate(
            &recovery, 60500, true, true, 60.0f, true, 45.0f));
}

TEST_CASE("BZM overheat recovery lowers settings within Bonanza limits",
          "[asic][bzm][overheat]")
{
    bzm_overheat_recovery_t recovery;
    uint16_t voltage_mv = 0;
    float frequency_mhz = 0.0f;
    TEST_ASSERT_TRUE(bzm_overheat_recovery_begin(
        &recovery, 0, 3000, 1250.0f));
    TEST_ASSERT_TRUE(bzm_overheat_recovery_reduced_targets(
        &recovery, 2100, 800.0f, &voltage_mv, &frequency_mhz));
    TEST_ASSERT_EQUAL_UINT16(2900, voltage_mv);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1150.0f, frequency_mhz);

    TEST_ASSERT_TRUE(bzm_overheat_recovery_begin(
        &recovery, 0, 2150, 850.0f));
    TEST_ASSERT_TRUE(bzm_overheat_recovery_reduced_targets(
        &recovery, 2100, 800.0f, &voltage_mv, &frequency_mhz));
    TEST_ASSERT_EQUAL_UINT16(2100, voltage_mv);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, frequency_mhz);
}

TEST_CASE("BZM overheat recovery rejects invalid state",
          "[asic][bzm][overheat]")
{
    bzm_overheat_recovery_t recovery;
    memset(&recovery, 0, sizeof(recovery));
    TEST_ASSERT_FALSE(bzm_overheat_recovery_begin(
        NULL, 0, 3000, 1200.0f));
    TEST_ASSERT_FALSE(bzm_overheat_recovery_begin(
        &recovery, 0, 0, 1200.0f));
    TEST_ASSERT_FALSE(bzm_overheat_recovery_begin(
        &recovery, 0, 3000, NAN));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_INACTIVE,
        bzm_overheat_recovery_evaluate(
            &recovery, 0, true, true, 50.0f, false, 0.0f));

    TEST_ASSERT_TRUE(bzm_overheat_recovery_begin(
        &recovery, 100, 3000, 1200.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_INVALID,
        bzm_overheat_recovery_evaluate(
            &recovery, 99, true, true, 50.0f, false, 0.0f));
    TEST_ASSERT_EQUAL(
        BZM_OVERHEAT_RECOVERY_WAIT_TELEMETRY,
        bzm_overheat_recovery_evaluate(
            &recovery, 30100, true, true, NAN, false, 0.0f));
}
