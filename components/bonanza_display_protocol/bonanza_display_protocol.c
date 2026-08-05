#include "bonanza_display_protocol.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#define BONANZA_DISPLAY_REG_LAST_BYTE \
    (BONANZA_DISPLAY_REG_FAN_PERCENT + sizeof(uint32_t) - 1U)

_Static_assert(
    BONANZA_DISPLAY_PACKET_SIZE ==
        1U + BONANZA_DISPLAY_REG_LAST_BYTE -
            BONANZA_DISPLAY_REG_DEVICE_FAMILY + 1U,
    "Unexpected bonanzaDisplay packet size");

static size_t packet_index(uint8_t reg)
{
    return 1U + reg - BONANZA_DISPLAY_REG_DEVICE_FAMILY;
}

static void packet_write_string(uint8_t *packet, uint8_t reg,
                                size_t field_size, const char *value,
                                bool uppercase)
{
    if (value == NULL) {
        return;
    }

    size_t offset = packet_index(reg);
    size_t length = strnlen(value, field_size - 1U);
    for (size_t i = 0; i < length; i++) {
        unsigned char character = (unsigned char)value[i];
        packet[offset + i] = uppercase
            ? (uint8_t)toupper(character)
            : (uint8_t)character;
    }
}

static void packet_write_u32(uint8_t *packet, uint8_t reg, uint32_t value)
{
    size_t offset = packet_index(reg);
    packet[offset] = (uint8_t)value;
    packet[offset + 1U] = (uint8_t)(value >> 8);
    packet[offset + 2U] = (uint8_t)(value >> 16);
    packet[offset + 3U] = (uint8_t)(value >> 24);
}

static uint32_t metric_to_u32(float value)
{
    if (!isfinite(value) || value <= 0.0f) {
        return 0;
    }
    if (value >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)(value + 0.5f);
}

static uint32_t fan_percent_to_u32(float value)
{
    uint32_t percent = metric_to_u32(value);
    return percent > 100U ? 100U : percent;
}

bool bonanza_display_identity_supported(const uint8_t identity[3])
{
    return identity != NULL &&
           identity[0] == BONANZA_DISPLAY_PROTOCOL_VERSION &&
           identity[1] == BONANZA_DISPLAY_I2C_ADDRESS &&
           identity[2] == BONANZA_DISPLAY_REGISTER_FILE_SIZE;
}

void bonanza_display_build_metrics_packet(
    const bonanza_display_metrics_t *metrics,
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE])
{
    if (packet == NULL) {
        return;
    }

    memset(packet, 0, BONANZA_DISPLAY_PACKET_SIZE);
    packet[0] = BONANZA_DISPLAY_REG_DEVICE_FAMILY;
    if (metrics == NULL) {
        return;
    }

    packet_write_string(packet, BONANZA_DISPLAY_REG_DEVICE_FAMILY,
                        BONANZA_DISPLAY_DEVICE_FAMILY_SIZE,
                        metrics->device_family, true);
    packet_write_string(packet, BONANZA_DISPLAY_REG_DEVICE_MODEL,
                        BONANZA_DISPLAY_DEVICE_MODEL_SIZE,
                        metrics->device_model, false);
    packet_write_string(packet, BONANZA_DISPLAY_REG_DEVICE_NAME,
                        BONANZA_DISPLAY_DEVICE_NAME_SIZE,
                        metrics->identify ? BONANZA_DISPLAY_IDENTIFY_TEXT
                                          : metrics->device_name,
                        false);
    packet_write_string(packet, BONANZA_DISPLAY_REG_IP_ADDRESS,
                        BONANZA_DISPLAY_IP_ADDRESS_SIZE,
                        metrics->ip_address, false);
    packet_write_string(packet, BONANZA_DISPLAY_REG_BEST_SHARE,
                        BONANZA_DISPLAY_BEST_SHARE_SIZE,
                        metrics->best_share, false);

    packet_write_u32(packet, BONANZA_DISPLAY_REG_HASHRATE_GHS,
                     metric_to_u32(metrics->hashrate_ghs));
    packet_write_u32(packet, BONANZA_DISPLAY_REG_TEMPERATURE_C,
                     metric_to_u32(metrics->temperature_c));
    packet_write_u32(packet, BONANZA_DISPLAY_REG_POWER_W,
                     metric_to_u32(metrics->power_w));
    packet_write_u32(packet, BONANZA_DISPLAY_REG_FREQUENCY_MHZ,
                     metric_to_u32(metrics->frequency_mhz));
    packet_write_u32(packet, BONANZA_DISPLAY_REG_FAN_PERCENT,
                     fan_percent_to_u32(metrics->fan_percent));
}
