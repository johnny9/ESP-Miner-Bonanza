#include "log_level_config.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"

static const char *const mining_debug_tags[] = {
    "asic_result",
    "scoreboard",
    "stratum_api",
    "stratum_v1_task",
    "stratum_v2_task",
};

bool log_level_config_parse(const char *name, esp_log_level_t *level)
{
    if (name == NULL || level == NULL) {
        return false;
    }

    if (strcmp(name, "ERROR") == 0) {
        *level = ESP_LOG_ERROR;
    } else if (strcmp(name, "WARN") == 0) {
        *level = ESP_LOG_WARN;
    } else if (strcmp(name, "INFO") == 0) {
        *level = ESP_LOG_INFO;
    } else if (strcmp(name, "DEBUG") == 0) {
        *level = ESP_LOG_DEBUG;
    } else {
        return false;
    }

    return true;
}

bool log_level_config_apply(const char *name)
{
    esp_log_level_t level;
    if (!log_level_config_parse(name, &level)) {
        return false;
    }

    const esp_log_level_t default_level =
        level == ESP_LOG_DEBUG ? ESP_LOG_INFO : level;
    esp_log_level_set("*", default_level);

    for (size_t i = 0; i < sizeof(mining_debug_tags) / sizeof(mining_debug_tags[0]); i++) {
        esp_log_level_set(mining_debug_tags[i], level);
    }
    return true;
}

bool log_level_config_is_valid(const char *name)
{
    esp_log_level_t level;
    return log_level_config_parse(name, &level);
}
