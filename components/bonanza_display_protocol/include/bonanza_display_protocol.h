#ifndef BONANZA_DISPLAY_PROTOCOL_H_
#define BONANZA_DISPLAY_PROTOCOL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BONANZA_DISPLAY_I2C_ADDRESS        0x3CU
#define BONANZA_DISPLAY_PROTOCOL_VERSION   1U
#define BONANZA_DISPLAY_REGISTER_FILE_SIZE 128U

#define BONANZA_DISPLAY_REG_PROTOCOL_VERSION 0x00U
#define BONANZA_DISPLAY_REG_DEVICE_FAMILY    0x10U
#define BONANZA_DISPLAY_REG_DEVICE_MODEL     0x20U
#define BONANZA_DISPLAY_REG_DEVICE_NAME      0x28U
#define BONANZA_DISPLAY_REG_IP_ADDRESS       0x38U
#define BONANZA_DISPLAY_REG_BEST_SHARE       0x48U
#define BONANZA_DISPLAY_REG_HASHRATE_GHS     0x60U
#define BONANZA_DISPLAY_REG_TEMPERATURE_C    0x64U
#define BONANZA_DISPLAY_REG_POWER_W          0x68U
#define BONANZA_DISPLAY_REG_FREQUENCY_MHZ    0x6CU
#define BONANZA_DISPLAY_REG_FAN_PERCENT      0x70U

#define BONANZA_DISPLAY_DEVICE_FAMILY_SIZE 16U
#define BONANZA_DISPLAY_DEVICE_MODEL_SIZE   8U
#define BONANZA_DISPLAY_DEVICE_NAME_SIZE    16U
#define BONANZA_DISPLAY_IP_ADDRESS_SIZE      16U
#define BONANZA_DISPLAY_BEST_SHARE_SIZE      16U

#define BONANZA_DISPLAY_IDENTIFY_TEXT "HI!"
#define BONANZA_DISPLAY_PACKET_SIZE 101U

typedef struct {
    const char *device_family;
    const char *device_model;
    const char *device_name;
    const char *ip_address;
    const char *best_share;
    float hashrate_ghs;
    float temperature_c;
    float power_w;
    float frequency_mhz;
    float fan_percent;
    bool identify;
} bonanza_display_metrics_t;

bool bonanza_display_identity_supported(const uint8_t identity[3]);

void bonanza_display_build_metrics_packet(
    const bonanza_display_metrics_t *metrics,
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE]);

#endif /* BONANZA_DISPLAY_PROTOCOL_H_ */
