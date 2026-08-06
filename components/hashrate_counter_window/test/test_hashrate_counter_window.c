#include "hashrate_counter_window.h"
#include "unity.h"

#include <limits.h>

TEST_CASE("counter window waits for thirty seconds", "[hashrate_counter_window]")
{
    hashrate_counter_window_t window = {0};
    uint32_t delta = 0;
    uint64_t duration_us = 0;

    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 100, 1000000, &delta, &duration_us));
    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 3000, 30000000, &delta, &duration_us));
    TEST_ASSERT_TRUE(hashrate_counter_window_update(&window, 3100, 31000000, &delta, &duration_us));
    TEST_ASSERT_EQUAL_UINT32(3000, delta);
    TEST_ASSERT_TRUE(duration_us == HASHRATE_COUNTER_WINDOW_US);
}

TEST_CASE("counter window advances while retaining thirty seconds", "[hashrate_counter_window]")
{
    hashrate_counter_window_t window = {0};
    uint32_t delta = 0;
    uint64_t duration_us = 0;

    for (uint64_t second = 1; second <= 32; second++) {
        bool ready = hashrate_counter_window_update(&window, (uint32_t) (second * 100), second * 1000000, &delta, &duration_us);
        if (second <= HASHRATE_COUNTER_WINDOW_SECONDS) {
            TEST_ASSERT_FALSE(ready);
        } else {
            TEST_ASSERT_TRUE(ready);
            TEST_ASSERT_EQUAL_UINT32(3000, delta);
            TEST_ASSERT_TRUE(duration_us == HASHRATE_COUNTER_WINDOW_US);
        }
    }
}

TEST_CASE("counter window handles uint32 wraparound", "[hashrate_counter_window]")
{
    hashrate_counter_window_t window = {0};
    uint32_t delta = 0;
    uint64_t duration_us = 0;

    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, UINT32_MAX - 10, 1000000, &delta, &duration_us));
    TEST_ASSERT_TRUE(hashrate_counter_window_update(&window, 9, 31000000, &delta, &duration_us));
    TEST_ASSERT_EQUAL_UINT32(20, delta);
}

TEST_CASE("counter window rejects non-monotonic timestamps", "[hashrate_counter_window]")
{
    hashrate_counter_window_t window = {0};
    uint32_t delta = 0;
    uint64_t duration_us = 0;

    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 10, 1000000, &delta, &duration_us));
    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 20, 1000000, &delta, &duration_us));
    TEST_ASSERT_EQUAL_UINT8(1, window.count);
}

TEST_CASE("counter window reset discards history", "[hashrate_counter_window]")
{
    hashrate_counter_window_t window = {0};
    uint32_t delta = 0;
    uint64_t duration_us = 0;

    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 100, 1000000, &delta, &duration_us));
    hashrate_counter_window_reset(&window);
    TEST_ASSERT_FALSE(hashrate_counter_window_update(&window, 3100, 31000000, &delta, &duration_us));
    TEST_ASSERT_EQUAL_UINT8(1, window.count);
}
