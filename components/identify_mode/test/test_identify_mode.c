#include <stdint.h>

#include "identify_mode.h"
#include "unity.h"

TEST_CASE("identify mode toggles and expires at its deadline", "[identify]")
{
    identify_mode_t mode = {0};

    TEST_ASSERT_FALSE(identify_mode_is_active(&mode, 1000));
    TEST_ASSERT_TRUE(identify_mode_toggle(&mode, 1000, 30000));
    TEST_ASSERT_TRUE(identify_mode_is_active(&mode, 1000));
    TEST_ASSERT_TRUE(identify_mode_is_active(&mode, 30999));
    TEST_ASSERT_FALSE(identify_mode_is_active(&mode, 31000));
    TEST_ASSERT_TRUE(identify_mode_toggle(&mode, 31000, 30000));
    TEST_ASSERT_TRUE(identify_mode_is_active(&mode, 31000));
}

TEST_CASE("identify mode can be toggled off or cancelled", "[identify]")
{
    identify_mode_t mode = {0};

    TEST_ASSERT_TRUE(identify_mode_toggle(&mode, 10, 30000));
    TEST_ASSERT_FALSE(identify_mode_toggle(&mode, 20, 30000));
    TEST_ASSERT_FALSE(identify_mode_is_active(&mode, 20));
    TEST_ASSERT_TRUE(identify_mode_toggle(&mode, 30, 30000));
    identify_mode_cancel(&mode);
    TEST_ASSERT_FALSE(identify_mode_is_active(&mode, 30));
}

TEST_CASE("identify mode handles the millisecond counter wrapping", "[identify]")
{
    identify_mode_t mode = {0};
    uint32_t start = UINT32_MAX - 10U;

    TEST_ASSERT_TRUE(identify_mode_toggle(&mode, start, 30));
    TEST_ASSERT_TRUE(identify_mode_is_active(&mode, 5));
    TEST_ASSERT_FALSE(identify_mode_is_active(&mode, 19));
}

TEST_CASE("identify mode rejects invalid calls", "[identify]")
{
    identify_mode_t mode = {0};

    TEST_ASSERT_FALSE(identify_mode_toggle(NULL, 0, 30000));
    TEST_ASSERT_FALSE(identify_mode_toggle(&mode, 0, 0));
    TEST_ASSERT_FALSE(identify_mode_is_active(NULL, 0));
    identify_mode_cancel(NULL);
}
