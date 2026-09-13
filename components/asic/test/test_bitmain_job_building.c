#include "unity.h"
#include "bm_job_builder.h"
#include "bitmain_job_test_support.h"
#include "utils.h"
#include <string.h>

void bitmain_assert_midstate_vector(const mining_template_t *work, unsigned index,
                                    const char *expected_hex)
{
    bm_job packet;
    TEST_ASSERT_TRUE(bm_job_build(work, &packet));
    TEST_ASSERT_EQUAL_UINT8(4, packet.num_midstates);
    TEST_ASSERT_LESS_THAN_UINT(4, index);
    uint8_t expected[32];
    TEST_ASSERT_EQUAL_UINT32(32, hex2bin(expected_hex, expected, sizeof(expected)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, packet.midstates[index], sizeof(expected));
}

TEST_CASE("Bitmain common-work conversion preserves explicit header fields", "[asic][job-building]")
{
    mining_template_t source = {
        .version = 0x20000000, .version_mask = 0, .target = 0x1705dd01,
        .ntime = 0x64658bd8, .starting_nonce = 123,
        .share = {.protocol = MINING_PROTOCOL_SV2_STANDARD, .pool_id = 3,
                  .pool_difficulty = 512, .job_id = "42", .extranonce2 = ""},
    };
    bm_job result;
    TEST_ASSERT_TRUE(bm_job_build(&source, &result));
    TEST_ASSERT_EQUAL_HEX32(source.version, result.version);
    TEST_ASSERT_EQUAL_HEX32(0, result.version_mask);
    TEST_ASSERT_EQUAL_HEX32(source.target, result.target);
    TEST_ASSERT_EQUAL_HEX32(source.ntime, result.ntime);
    TEST_ASSERT_EQUAL_UINT32(123, result.starting_nonce);
    source.version = 0x20002000;
    source.version_mask = 0x1fffe000;
    TEST_ASSERT_TRUE(bm_job_build(&source, &result));
    TEST_ASSERT_EQUAL_HEX32(source.version, result.version);
    TEST_ASSERT_EQUAL_HEX32(source.version_mask, result.version_mask);
    TEST_ASSERT_EQUAL_STRING("42", source.share.job_id);
}

TEST_CASE("Bitmain software midstates follow the common work mask", "[asic][job-building]")
{
    mining_template_t source = {.version = 0x20000000};
    bm_job first, rolled;
    TEST_ASSERT_TRUE(bm_job_build(&source, &first));
    TEST_ASSERT_EQUAL_UINT8(1, first.num_midstates);
    source.version_mask = 0x1fffe000;
    TEST_ASSERT_TRUE(bm_job_build(&source, &rolled));
    TEST_ASSERT_EQUAL_UINT8(4, rolled.num_midstates);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(first.midstate, rolled.midstate, 32);
    TEST_ASSERT_FALSE(memcmp(rolled.midstate, rolled.midstate1, 32) == 0);
    TEST_ASSERT_FALSE(bm_job_build(NULL, &rolled));
    TEST_ASSERT_FALSE(bm_job_build(&source, NULL));
}
