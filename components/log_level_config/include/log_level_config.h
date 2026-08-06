#ifndef LOG_LEVEL_CONFIG_H_
#define LOG_LEVEL_CONFIG_H_

#include <stdbool.h>

#include "esp_log.h"

#define LOG_LEVEL_CONFIG_DEFAULT "INFO"

bool log_level_config_parse(const char *name, esp_log_level_t *level);
bool log_level_config_is_valid(const char *name);
bool log_level_config_apply(const char *name);

#endif /* LOG_LEVEL_CONFIG_H_ */
