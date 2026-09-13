#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

static const char *level_name(esp_log_level_t level)
{
    switch (level) {
        case ESP_LOG_ERROR:
            return "E";
        case ESP_LOG_WARN:
            return "W";
        case ESP_LOG_DEBUG:
            return "D";
        case ESP_LOG_VERBOSE:
            return "V";
        case ESP_LOG_INFO:
        case ESP_LOG_NONE:
        default:
            return "I";
    }
}

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...)
{
    va_list arguments;

    fprintf(stderr, "%s (%s): ", level_name(level), tag != NULL ? tag : "");
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

void host_esp_log_buffer_hex(esp_log_level_t level, const char *tag,
                             const void *buffer, size_t length)
{
    const uint8_t *bytes = buffer;

    fprintf(stderr, "%s (%s):", level_name(level), tag != NULL ? tag : "");
    for (size_t index = 0; index < length; ++index) {
        fprintf(stderr, " %02x", bytes[index]);
    }
    fputc('\n', stderr);
}

/* Model ESP-IDF's wildcard default and per-tag overrides for shared log tests. */
static esp_log_level_t default_log_level = ESP_LOG_INFO;
static struct {
    char tag[64];
    esp_log_level_t level;
} log_levels[64];
static size_t log_level_count;

void esp_log_level_set(const char *tag, esp_log_level_t level)
{
    if (strcmp(tag, "*") == 0) {
        default_log_level = level;
        log_level_count = 0;
        return;
    }
    for (size_t i = 0; i < log_level_count; ++i) {
        if (strcmp(log_levels[i].tag, tag) == 0) {
            log_levels[i].level = level;
            return;
        }
    }
    if (log_level_count >= sizeof(log_levels) / sizeof(log_levels[0]) ||
        strlen(tag) >= sizeof(log_levels[0].tag)) abort();
    strcpy(log_levels[log_level_count].tag, tag);
    log_levels[log_level_count++].level = level;
}

esp_log_level_t esp_log_level_get(const char *tag)
{
    for (size_t i = 0; i < log_level_count; ++i) {
        if (strcmp(log_levels[i].tag, tag) == 0) return log_levels[i].level;
    }
    return default_log_level;
}
