#include "unity.h"
#include "bitmain_job_packet.h"
#include "bitmain_job_test_support.h"
#include "utils.h"
#include <string.h>

void bitmain_assert_midstate_vector(const asic_job_t *work, unsigned index,
                                    const char *expected_hex)
{
    bm1397_job_packet_t packet;
    bm1397_build_job_packet(work, 4, 4, &packet);
    TEST_ASSERT_EQUAL_UINT8(4, packet.num_midstates);
    TEST_ASSERT_LESS_THAN_UINT(4, index);
    uint8_t expected[32];
    TEST_ASSERT_EQUAL_UINT32(32, hex2bin(expected_hex, expected, sizeof(expected)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, packet.midstates[index], sizeof(expected));
}

TEST_CASE("Bitmain software midstates follow the common work mask", "[asic][job-building]")
{
    asic_job_t source = {.version = 0x20000000};
    bm1397_job_packet_t first, rolled;
    bm1397_build_job_packet(&source, 4, 4, &first);
    TEST_ASSERT_EQUAL_UINT8(1, first.num_midstates);
    source.version_mask = 0x1fffe000;
    bm1397_build_job_packet(&source, 4, 4, &rolled);
    TEST_ASSERT_EQUAL_UINT8(4, rolled.num_midstates);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(first.midstates[0], rolled.midstates[0], 32);
    TEST_ASSERT_FALSE(memcmp(rolled.midstates[0], rolled.midstates[1], 32) == 0);
}
