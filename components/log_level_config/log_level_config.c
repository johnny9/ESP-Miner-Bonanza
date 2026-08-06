#include "log_level_config.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"

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

    esp_log_level_set("*", level);
    return true;
}

bool log_level_config_is_valid(const char *name)
{
    esp_log_level_t level;
    return log_level_config_parse(name, &level);
}
