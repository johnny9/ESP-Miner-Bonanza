#include "log_level_config.h"
#include "unity.h"

TEST_CASE("supported log levels map to ESP-IDF levels", "[log_level_config]")
{
    esp_log_level_t level = ESP_LOG_NONE;

    TEST_ASSERT_TRUE(log_level_config_parse("ERROR", &level));
    TEST_ASSERT_EQUAL(ESP_LOG_ERROR, level);
    TEST_ASSERT_TRUE(log_level_config_parse("WARN", &level));
    TEST_ASSERT_EQUAL(ESP_LOG_WARN, level);
    TEST_ASSERT_TRUE(log_level_config_parse("INFO", &level));
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, level);
    TEST_ASSERT_TRUE(log_level_config_parse("DEBUG", &level));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, level);
}

TEST_CASE("unsupported log levels are rejected", "[log_level_config]")
{
    esp_log_level_t level = ESP_LOG_INFO;

    TEST_ASSERT_FALSE(log_level_config_parse(NULL, &level));
    TEST_ASSERT_FALSE(log_level_config_parse("INFO", NULL));
    TEST_ASSERT_FALSE(log_level_config_parse("", &level));
    TEST_ASSERT_FALSE(log_level_config_parse("NONE", &level));
    TEST_ASSERT_FALSE(log_level_config_parse("VERBOSE", &level));
    TEST_ASSERT_FALSE(log_level_config_parse("debug", &level));
    TEST_ASSERT_TRUE(log_level_config_is_valid(LOG_LEVEL_CONFIG_DEFAULT));
    TEST_ASSERT_FALSE(log_level_config_is_valid("VERBOSE"));
}
