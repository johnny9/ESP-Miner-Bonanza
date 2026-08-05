#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bonanza_display.h"
#include "bonanza_display_protocol.h"
#include "global_state.h"
#include "identify_mode.h"
#include "nvs_config.h"

#define BONANZA_I2C_SPEED_HZ         100000U
// Dedicated bonanzaDisplay FPC bus; the miner PMBus remains on GPIO47/48.
#define BONANZA_I2C_PORT             I2C_NUM_1
#define BONANZA_I2C_SDA_GPIO         GPIO_NUM_7
#define BONANZA_I2C_SCL_GPIO         GPIO_NUM_6
#define BONANZA_IO_TIMEOUT_MS        250
#define BONANZA_REFRESH_MS           1000
#define BONANZA_RECONNECT_MS         5000

static const char * TAG = "bonanza_display";

static i2c_master_bus_handle_t bonanza_bus_handle;
static i2c_master_dev_handle_t bonanza_device_handle;
static char configured_device_name[BONANZA_DISPLAY_DEVICE_NAME_SIZE] = "bitaxe";

static void build_metrics_packet(
    const GlobalState *global_state,
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE])
{
    const SystemModule * system = &global_state->SYSTEM_MODULE;
    const PowerManagementModule * power = &global_state->POWER_MANAGEMENT_MODULE;

    const char * device_name = system->mdns_hostname[0] != '\0'
        ? system->mdns_hostname
        : configured_device_name;

    float temperature = power->chip_temp_avg;
    if (power->chip_temp2_avg > temperature) {
        temperature = power->chip_temp2_avg;
    }

    bonanza_display_metrics_t metrics = {
        .device_family = global_state->DEVICE_CONFIG.family.name,
        .device_model = global_state->DEVICE_CONFIG.board_version,
        .device_name = device_name,
        .ip_address = system->ip_addr_str,
        .best_share = system->best_diff_string,
        .hashrate_ghs = system->current_hashrate,
        .temperature_c = temperature,
        .power_w = power->power,
        .frequency_mhz = power->actual_frequency,
        .fan_percent = power->fan_perc,
        .identify = identify_mode_is_active(
            &system->identify_mode,
            (uint32_t)(esp_timer_get_time() / 1000)),
    };
    bonanza_display_build_metrics_packet(&metrics, packet);
}

static esp_err_t probe_display(void)
{
    const uint8_t register_address = BONANZA_DISPLAY_REG_PROTOCOL_VERSION;
    uint8_t identity[3] = {0};
    esp_err_t error = i2c_master_transmit_receive(
        bonanza_device_handle,
        &register_address,
        sizeof(register_address),
        identity,
        sizeof(identity),
        BONANZA_IO_TIMEOUT_MS);

    if (error != ESP_OK) {
        return error;
    }
    if (!bonanza_display_identity_supported(identity)) {
        ESP_LOGW(TAG, "Unsupported device at 0x%02X: protocol=%u address=0x%02X registers=%u",
                 BONANZA_DISPLAY_I2C_ADDRESS,
                 identity[0], identity[1], identity[2]);
        return ESP_ERR_INVALID_VERSION;
    }

    return ESP_OK;
}

static esp_err_t publish_metrics(const GlobalState * global_state)
{
    uint8_t packet[BONANZA_DISPLAY_PACKET_SIZE];
    build_metrics_packet(global_state, packet);
    return i2c_master_transmit(
        bonanza_device_handle,
        packet,
        sizeof(packet),
        BONANZA_IO_TIMEOUT_MS);
}

static void bonanza_display_task(void * pvParameters)
{
    GlobalState * global_state = (GlobalState *) pvParameters;
    SystemModule *system = &global_state->SYSTEM_MODULE;
    bool connected = false;
    bool absence_logged = false;
    system->external_display_connected = false;

    while (true) {
        if (!connected) {
            esp_err_t error = probe_display();
            if (error != ESP_OK) {
                if (!absence_logged) {
                    ESP_LOGW(TAG, "bonanzaDisplay unavailable; mining will continue and the display will be retried");
                    absence_logged = true;
                }
                system->external_display_connected = false;
                vTaskDelay(pdMS_TO_TICKS(BONANZA_RECONNECT_MS));
                continue;
            }

            connected = true;
            absence_logged = false;
            ESP_LOGI(TAG, "Connected to bonanzaDisplay protocol v%d at 0x%02X",
                     BONANZA_DISPLAY_PROTOCOL_VERSION,
                     BONANZA_DISPLAY_I2C_ADDRESS);
        }

        esp_err_t error = publish_metrics(global_state);
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "bonanzaDisplay disconnected: %s", esp_err_to_name(error));
            system->external_display_connected = false;
            connected = false;
            vTaskDelay(pdMS_TO_TICKS(BONANZA_RECONNECT_MS));
            continue;
        }
        system->external_display_connected = true;

        vTaskDelay(pdMS_TO_TICKS(BONANZA_REFRESH_MS));
    }
}

esp_err_t bonanza_display_init(void * pvParameters)
{
    GlobalState *global_state = (GlobalState *)pvParameters;
    global_state->SYSTEM_MODULE.external_display_connected = false;

    char * hostname = nvs_config_get_string(NVS_CONFIG_HOSTNAME);
    if (hostname != NULL) {
        size_t length = strnlen(hostname, sizeof(configured_device_name) - 1);
        memcpy(configured_device_name, hostname, length);
        configured_device_name[length] = '\0';
        free(hostname);
    }

    const i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BONANZA_I2C_PORT,
        .scl_io_num = BONANZA_I2C_SCL_GPIO,
        .sda_io_num = BONANZA_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bonanza_bus_handle),
                        TAG, "Failed to initialize bonanzaDisplay I2C bus");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BONANZA_DISPLAY_I2C_ADDRESS,
        .scl_speed_hz = BONANZA_I2C_SPEED_HZ,
    };
    esp_err_t error = i2c_master_bus_add_device(
        bonanza_bus_handle, &device_config, &bonanza_device_handle);
    if (error != ESP_OK) {
        i2c_del_master_bus(bonanza_bus_handle);
        bonanza_bus_handle = NULL;
        ESP_RETURN_ON_ERROR(error, TAG, "Failed to add bonanzaDisplay I2C device");
    }

    ESP_LOGI(TAG, "bonanzaDisplay I2C bus initialized: SDA=GPIO%d SCL=GPIO%d speed=%u Hz",
             BONANZA_I2C_SDA_GPIO, BONANZA_I2C_SCL_GPIO, BONANZA_I2C_SPEED_HZ);

    if (xTaskCreate(bonanza_display_task, "bonanza display", 4096, pvParameters, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create bonanzaDisplay task");
        i2c_master_bus_rm_device(bonanza_device_handle);
        bonanza_device_handle = NULL;
        i2c_del_master_bus(bonanza_bus_handle);
        bonanza_bus_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
