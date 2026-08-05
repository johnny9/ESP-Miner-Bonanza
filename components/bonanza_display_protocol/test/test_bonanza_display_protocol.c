#include <math.h>
#include <stdint.h>
#include <string.h>

#include "bonanza_display_protocol.h"
#include "unity.h"

static size_t packet_index(uint8_t reg)
{
    return 1U + reg - BONANZA_DISPLAY_REG_DEVICE_FAMILY;
}

static uint32_t packet_read_u32(const uint8_t *packet, uint8_t reg)
{
    size_t offset = packet_index(reg);
    return (uint32_t)packet[offset] |
           ((uint32_t)packet[offset + 1U] << 8) |
           ((uint32_t)packet[offset + 2U] << 16) |
           ((uint32_t)packet[offset + 3U] << 24);
}

static bonanza_display_metrics_t sample_metrics(void)
{
    return (bonanza_display_metrics_t){
        .device_family = "Bonanza",
        .device_model = "1002",
        .device_name = "bonanza.local",
        .ip_address = "192.168.1.186",
        .best_share = "42.5M",
        .hashrate_ghs = 1496.6f,
        .temperature_c = 60.4f,
        .power_w = 47.1f,
        .frequency_mhz = 1200.0f,
        .fan_percent = 73.0f,
    };
}

TEST_CASE("bonanzaDisplay identity requires the complete protocol tuple",
          "[bonanza_display]")
{
    uint8_t identity[3] = {
        BONANZA_DISPLAY_PROTOCOL_VERSION,
        BONANZA_DISPLAY_I2C_ADDRESS,
        BONANZA_DISPLAY_REGISTER_FILE_SIZE,
    };

    TEST_ASSERT_TRUE(bonanza_display_identity_supported(identity));
    identity[0]++;
    TEST_ASSERT_FALSE(bonanza_display_identity_supported(identity));
    identity[0] = BONANZA_DISPLAY_PROTOCOL_VERSION;
    identity[1]++;
    TEST_ASSERT_FALSE(bonanza_display_identity_supported(identity));
    identity[1] = BONANZA_DISPLAY_I2C_ADDRESS;
    identity[2]--;
    TEST_ASSERT_FALSE(bonanza_display_identity_supported(identity));
    TEST_ASSERT_FALSE(bonanza_display_identity_supported(NULL));
}

TEST_CASE("bonanzaDisplay packet matches the 1002x register map",
          "[bonanza_display]")
{
    bonanza_display_metrics_t metrics = sample_metrics();
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE];

    bonanza_display_build_metrics_packet(&metrics, packet);

    TEST_ASSERT_EQUAL_HEX8(BONANZA_DISPLAY_REG_DEVICE_FAMILY, packet[0]);
    TEST_ASSERT_EQUAL_STRING(
        "BONANZA",
        (char *)&packet[packet_index(BONANZA_DISPLAY_REG_DEVICE_FAMILY)]);
    TEST_ASSERT_EQUAL_STRING(
        "1002",
        (char *)&packet[packet_index(BONANZA_DISPLAY_REG_DEVICE_MODEL)]);
    TEST_ASSERT_EQUAL_STRING(
        "bonanza.local",
        (char *)&packet[packet_index(BONANZA_DISPLAY_REG_DEVICE_NAME)]);
    TEST_ASSERT_EQUAL_STRING(
        "192.168.1.186",
        (char *)&packet[packet_index(BONANZA_DISPLAY_REG_IP_ADDRESS)]);
    TEST_ASSERT_EQUAL_STRING(
        "42.5M",
        (char *)&packet[packet_index(BONANZA_DISPLAY_REG_BEST_SHARE)]);
    TEST_ASSERT_EQUAL_UINT32(
        1497, packet_read_u32(packet, BONANZA_DISPLAY_REG_HASHRATE_GHS));
    TEST_ASSERT_EQUAL_UINT32(
        60, packet_read_u32(packet, BONANZA_DISPLAY_REG_TEMPERATURE_C));
    TEST_ASSERT_EQUAL_UINT32(
        47, packet_read_u32(packet, BONANZA_DISPLAY_REG_POWER_W));
    TEST_ASSERT_EQUAL_UINT32(
        1200, packet_read_u32(packet, BONANZA_DISPLAY_REG_FREQUENCY_MHZ));
    TEST_ASSERT_EQUAL_UINT32(
        73, packet_read_u32(packet, BONANZA_DISPLAY_REG_FAN_PERCENT));
}

TEST_CASE("bonanzaDisplay identify overrides only the visible device name",
          "[bonanza_display]")
{
    bonanza_display_metrics_t metrics = sample_metrics();
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE];
    metrics.identify = true;

    bonanza_display_build_metrics_packet(&metrics, packet);

    size_t name_offset = packet_index(BONANZA_DISPLAY_REG_DEVICE_NAME);
    TEST_ASSERT_EQUAL_STRING(BONANZA_DISPLAY_IDENTIFY_TEXT,
                             (char *)&packet[name_offset]);
    for (size_t i = strlen(BONANZA_DISPLAY_IDENTIFY_TEXT);
         i < BONANZA_DISPLAY_DEVICE_NAME_SIZE; i++) {
        TEST_ASSERT_EQUAL_HEX8(0, packet[name_offset + i]);
    }
    TEST_ASSERT_EQUAL_UINT32(
        1497, packet_read_u32(packet, BONANZA_DISPLAY_REG_HASHRATE_GHS));
}

TEST_CASE("bonanzaDisplay metrics reject invalid values and clamp fan percent",
          "[bonanza_display]")
{
    bonanza_display_metrics_t metrics = sample_metrics();
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE];
    metrics.hashrate_ghs = NAN;
    metrics.temperature_c = -1.0f;
    metrics.power_w = INFINITY;
    metrics.frequency_mhz = 5.0e9f;
    metrics.fan_percent = 150.0f;

    bonanza_display_build_metrics_packet(&metrics, packet);

    TEST_ASSERT_EQUAL_UINT32(
        0, packet_read_u32(packet, BONANZA_DISPLAY_REG_HASHRATE_GHS));
    TEST_ASSERT_EQUAL_UINT32(
        0, packet_read_u32(packet, BONANZA_DISPLAY_REG_TEMPERATURE_C));
    TEST_ASSERT_EQUAL_UINT32(
        0, packet_read_u32(packet, BONANZA_DISPLAY_REG_POWER_W));
    TEST_ASSERT_EQUAL_UINT32(
        UINT32_MAX,
        packet_read_u32(packet, BONANZA_DISPLAY_REG_FREQUENCY_MHZ));
    TEST_ASSERT_EQUAL_UINT32(
        100, packet_read_u32(packet, BONANZA_DISPLAY_REG_FAN_PERCENT));
}

TEST_CASE("bonanzaDisplay packet is deterministic with missing metrics",
          "[bonanza_display]")
{
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE];
    memset(packet, 0xA5, sizeof(packet));

    bonanza_display_build_metrics_packet(NULL, packet);

    TEST_ASSERT_EQUAL_HEX8(BONANZA_DISPLAY_REG_DEVICE_FAMILY, packet[0]);
    for (size_t i = 1; i < sizeof(packet); i++) {
        TEST_ASSERT_EQUAL_HEX8(0, packet[i]);
    }
}
