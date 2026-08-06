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

TEST_CASE("DEBUG is limited to mining diagnostics", "[log_level_config]")
{
    TEST_ASSERT_TRUE(log_level_config_apply("DEBUG"));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_level_get("asic_result"));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_level_get("scoreboard"));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_level_get("stratum_api"));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_level_get("stratum_v1_task"));
    TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_level_get("stratum_v2_task"));
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, esp_log_level_get("httpd_txrx"));

    TEST_ASSERT_TRUE(log_level_config_apply(LOG_LEVEL_CONFIG_DEFAULT));
    TEST_ASSERT_EQUAL(ESP_LOG_INFO, esp_log_level_get("asic_result"));
}
