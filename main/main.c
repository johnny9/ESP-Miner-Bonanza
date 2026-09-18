#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_psram.h"

#include "adc.h"
#include "asic.h"
#include "asic_init.h"
#include "asic_reset.h"
#include "asic_reset_backend.h"
#include "asic_result_task.h"
#include "bap/bap.h"
#include "bzm_bridge_update.h"
#include "power_management_task.h"
#include "connect.h"
#include "create_jobs_task.h"
#include "device_config.h"
#include "fan_controller_task.h"
#include "filesystem.h"
#include "hashrate_monitor_task.h"
#include "http_server.h"
#include "i2c_bitaxe.h"
#include "input.h"
#include "log_buffer.h"
#include "log_level_config.h"
#include "nvs_config.h"
#include "stratum_task.h"
#include "stratum_submission.h"
#include "miner_job.h"
#include "esp_netif_sntp.h"
#include "self_test.h"
#include "serial.h"
#include "statistics_task.h"
#include "system.h"
#include "task_monitor.h"
#include "setup_ble.h"

static GlobalState GLOBAL_STATE;

static const char * TAG = "bitaxe";

#define DEFAULT_GPIO_I2C_SDA CONFIG_GPIO_I2C_SDA
#define DEFAULT_GPIO_I2C_SCL CONFIG_GPIO_I2C_SCL

static void heap_alloc_failed_hook(size_t requested_size, uint32_t caps, const char *function_name)
{
    if (caps & MALLOC_CAP_SPIRAM) {
        ESP_EARLY_LOGE(TAG, "%s failed to allocate %zu bytes from PSRAM", function_name, requested_size);
        abort();
    }
}

static void * cjson_malloc_psram(size_t size)
{
    if (esp_psram_is_initialized()) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    return malloc(size);
}

static void cjson_free_psram(void * ptr)
{
    free(ptr);
}

void app_main(void)
{
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(heap_alloc_failed_hook));

    cJSON_Hooks hooks = {.malloc_fn = cjson_malloc_psram, .free_fn = cjson_free_psram};
    cJSON_InitHooks(&hooks);
    if (esp_psram_is_initialized()) {
        GLOBAL_STATE.psram_is_available = true;
        log_buffer_init();
    } else {
        ESP_LOGE(TAG, "No PSRAM available on ESP32 device!");
    }

    ESP_LOGI(TAG, "Welcome to the bitaxe - FOSS || GTFO!");

    if (xTaskCreateWithCaps(cpu_monitor_task, "cpu_monitor", 4096, (void *) &GLOBAL_STATE, 1, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGE(TAG, "Error creating cpu monitor task");
    }
#ifdef CONFIG_ENABLE_TASK_MONITOR
    if (xTaskCreateWithCaps(task_monitor_task, "task_monitor", 8192, NULL, 1, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGE(TAG, "Error creating task monitor task");
    }
#endif
    // Board identity is loaded before driving reset or initializing board I2C.
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // Init ADC
    ADC_init();

    // initialize the ESP32 NVS
    if (nvs_config_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS");
        return;
    }

    char *configured_log_level = nvs_config_get_string(NVS_CONFIG_LOG_LEVEL);
    if (!log_level_config_apply(configured_log_level)) {
        ESP_LOGW(TAG, "Invalid configured log level; using %s",
                 LOG_LEVEL_CONFIG_DEFAULT);
        nvs_config_set_string(NVS_CONFIG_LOG_LEVEL, LOG_LEVEL_CONFIG_DEFAULT);
        log_level_config_apply(LOG_LEVEL_CONFIG_DEFAULT);
    }
    free(configured_log_level);
    // Check firmware version migration (resets useCustomWWW on update/downgrade)
    SYSTEM_check_firmware_migration();

    // Confirm app validity for OTA rollback
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First boot after OTA update, confirming app validity");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    // Ensure SSID is initialized before any screen/self-test uses it.
    GLOBAL_STATE.SYSTEM_MODULE.ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID);
    if (GLOBAL_STATE.SYSTEM_MODULE.ssid == NULL) {
        ESP_LOGW(TAG, "No SSID configured in NVS, using empty string");
        GLOBAL_STATE.SYSTEM_MODULE.ssid = strdup("");
        if (GLOBAL_STATE.SYSTEM_MODULE.ssid == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for SSID");
            return;
        }
    }

    if (device_config_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init device config");
        return;
    }

    // Board identity decides whether reset is a direct GPIO or is owned by
    // the Bonanza RP2040 bridge. Never drive GPIO1 before that decision.
    ESP_ERROR_CHECK(asic_reset_configure(GLOBAL_STATE.DEVICE_CONFIG.bonanza_bridge
        ? &ASIC_RESET_BRIDGE_BACKEND : &ASIC_RESET_GPIO_BACKEND));
    esp_err_t reset_safe_err = asic_hold_reset_low();
    if (reset_safe_err == ESP_OK) {
        ESP_LOGI(TAG, "ASIC reset initialized to the safe state");
    } else if (bzm_bridge_update_boot_recovery_allowed(
                   &GLOBAL_STATE.DEVICE_CONFIG, reset_safe_err)) {
        /*
         * A factory-blank RP2040 cannot acknowledge this command. Continue
         * booting so Wi-Fi, AxeOS, and the onboard SWD recovery endpoint stay
         * available. Board power management remains fail-closed and will not
         * energize or dispatch work without coherent bridge safety evidence.
         */
        GLOBAL_STATE.SYSTEM_MODULE.mining_paused = true;
        ESP_LOGE(TAG,
                 "Bonanza bridge unavailable during early safe-state request: "
                 "%s; continuing for HTTP bridge recovery",
                 esp_err_to_name(reset_safe_err));
    } else {
        ESP_ERROR_CHECK(reset_safe_err);
    }

    // Init I2C
    if (GLOBAL_STATE.DEVICE_CONFIG.pins.i2c != NULL) {
        ESP_ERROR_CHECK(i2c_bitaxe_init(GLOBAL_STATE.DEVICE_CONFIG.pins.i2c->sda, GLOBAL_STATE.DEVICE_CONFIG.pins.i2c->scl));
        ESP_LOGI(TAG, "I2C initialized successfully");
    } else {
        ESP_LOGI(TAG, "I2C pins not configured for board; skipping I2C initialization");
    }

    // wait for I2C to init
    vTaskDelay(100 / portTICK_PERIOD_MS);

    if (self_test_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init self test");
        return;
    }

    SYSTEM_init_system(&GLOBAL_STATE);
    if (scoreboard_init(&GLOBAL_STATE.SYSTEM_MODULE.scoreboard) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init scoreboard");
    }

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        wifi_init(&GLOBAL_STATE);
    }

    esp_err_t system_init_ret = SYSTEM_init_peripherals(&GLOBAL_STATE);
    SYSTEM_init_versions(&GLOBAL_STATE);

    if (system_init_ret == ESP_OK) {
        system_init_ret = POWER_MANAGEMENT_init(&GLOBAL_STATE);
        if (system_init_ret == ESP_OK && !GLOBAL_STATE.SELF_TEST_MODULE.is_active &&
            xTaskCreateWithCaps(FAN_CONTROLLER_task, "fan_controller", 8192,
                &GLOBAL_STATE, 10, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
            system_init_ret = ESP_ERR_NO_MEM;
            GLOBAL_STATE.SYSTEM_MODULE.hardware_fault = true;
            ESP_LOGE(TAG, "Required fan controller task could not start");
        }
    }
    if (system_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Peripheral/power initialization failed: %s", esp_err_to_name(system_init_ret));
    }

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        // start the API for AxeOS
        start_rest_server(&GLOBAL_STATE);
    }

    // Pre-cache partition descriptions and space usage percentage
    SYSTEM_init_partitions(&GLOBAL_STATE);

    // UART2 is the Bonanza control link on board 1002. Other products keep
    // using it for the Bitaxe Accessory Port.
    if (!GLOBAL_STATE.DEVICE_CONFIG.bonanza_bridge) {
        esp_err_t bap_ret = BAP_init(&GLOBAL_STATE);
        if (bap_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize BAP interface: %d", bap_ret);
            // Continue anyway, as BAP is not critical for core functionality
        }
    } else {
        ESP_LOGI(TAG, "UART2 reserved for the Bonanza RP2040 bridge; BAP disabled");
    }

    // While the device is still in setup mode (config AP up but no WiFi
    // connection), expose the BLE provisioning service so the miner can be
    // configured over Bluetooth. A short grace period avoids spinning up BLE on
    // a normal boot that connects within a few seconds. setup_ble_start() is
    // idempotent and only takes effect once the AP is actually enabled.
    int setup_ble_grace_ms = 0;
    while (!GLOBAL_STATE.SYSTEM_MODULE.is_connected) {
        if (GLOBAL_STATE.SYSTEM_MODULE.ap_enabled && setup_ble_grace_ms >= 5000) {
            setup_ble_start(&GLOBAL_STATE);
        }
        setup_ble_grace_ms += 100;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Connected to WiFi: tear down the setup BLE service to free the radio.
    setup_ble_stop();

    if (nvs_config_get_bool(NVS_CONFIG_USE_NTP)) {
        ESP_LOGI(TAG, "Starting SNTP");
        // default to pool.ntp.org to find the nearest NTP server if none are provided by DHCP
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        config.start = true;
        config.smooth_sync = true;
        config.server_from_dhcp = true;
        config.renew_servers_after_new_IP = true; // replace default with DHCP-provided server(s)
        config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
        esp_netif_sntp_init(&config);

        int retry = 15;
        while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && --retry >= 0) {
            ESP_LOGI(TAG, "Waiting for NTP... (%d attempts remaining)", retry);
        }
        if (retry == -1) {
            ESP_LOGW(TAG, "Failed to get NTP in time! Certificate validation may fail!");
        }
    }

    miner_job_pool_init();

    if (system_init_ret == ESP_OK) {
        /* Common task stack for every board. Power management alone starts
         * hardware, after all required consumers exist. Dispatch stays shut
         * until its board startup operation succeeds. */
        bool tasks_ready =
            xTaskCreate(create_jobs_task, "stratum miner", 8192, &GLOBAL_STATE,
                20, &GLOBAL_STATE.create_jobs_task_handle) == pdPASS;
        tasks_ready = xTaskCreateWithCaps(ASIC_result_task, "asic result", 8192,
            &GLOBAL_STATE, 15, NULL, MALLOC_CAP_SPIRAM) == pdPASS && tasks_ready;
        tasks_ready = xTaskCreateWithCaps(hashrate_monitor_task, "hashrate monitor", 8192,
            &GLOBAL_STATE, 5, NULL, MALLOC_CAP_SPIRAM) == pdPASS && tasks_ready;
        tasks_ready = xTaskCreateWithCaps(statistics_task, "statistics", 8192,
            &GLOBAL_STATE, 3, NULL, MALLOC_CAP_SPIRAM) == pdPASS && tasks_ready;
        if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
            tasks_ready = stratum_submission_init(&GLOBAL_STATE) && tasks_ready;
            if (GLOBAL_STATE.stratum_share_queue) {
                tasks_ready = xTaskCreateWithCaps(stratum_submission_task, "stratum submit", 8192,
                    &GLOBAL_STATE, 5, NULL, MALLOC_CAP_SPIRAM) == pdPASS && tasks_ready;
            }
            tasks_ready = xTaskCreateWithCaps(stratum_task, "stratum", 16384,
                &GLOBAL_STATE, 5, NULL, MALLOC_CAP_SPIRAM) == pdPASS && tasks_ready;
        }
        if (tasks_ready) {
            POWER_MANAGEMENT_set_ready();
            if (GLOBAL_STATE.SELF_TEST_MODULE.is_active &&
                !POWER_MANAGEMENT_wait_started(180000)) {
                system_init_ret = ESP_FAIL;
                ESP_LOGE(TAG, "Power management kept the ASIC off; recovery services remain available");
            }
        } else {
            system_init_ret = ESP_ERR_NO_MEM;
            GLOBAL_STATE.SYSTEM_MODULE.hardware_fault = true;
            ESP_LOGE(TAG, "Required mining task could not start; hardware remains off");
        }
    }

    if (GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        GLOBAL_STATE.SELF_TEST_MODULE.system_init_ret = system_init_ret;
        if (xTaskCreateWithCaps(self_test_task, "self_test", 8192, (void *) &GLOBAL_STATE, 10, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
            ESP_LOGE(TAG, "Error creating self test task");
        }
    }
}
