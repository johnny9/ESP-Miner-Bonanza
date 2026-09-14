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
#include "asic_result_task.h"
#include "bap/bap.h"
#include "bzm_bridge_update.h"
#include "bzm_controller.h"
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
    esp_err_t reset_safe_err = asic_hold_reset_low(&GLOBAL_STATE);
    if (reset_safe_err == ESP_OK) {
        ESP_LOGI(TAG, "ASIC reset initialized to the safe state");
    } else if (bzm_bridge_update_boot_recovery_allowed(
                   &GLOBAL_STATE.DEVICE_CONFIG, reset_safe_err)) {
        /*
         * A factory-blank RP2040 cannot acknowledge this command. Continue
         * booting so Wi-Fi, AxeOS, and the onboard SWD recovery endpoint stay
         * available. The Bonanza controller remains fail-closed and will not
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
        if (GLOBAL_STATE.DEVICE_CONFIG.bonanza_bridge) {
            /*
             * Board 1002 uses its dedicated production controller instead of
             * the legacy voltage and frequency management task.
             */
            esp_err_t runtime_err = bzm_controller_init(&GLOBAL_STATE);
            if (runtime_err != ESP_OK) {
                ESP_LOGE(TAG, "Bonanza safe-off runtime initialization failed: %s", esp_err_to_name(runtime_err));
                system_init_ret = runtime_err;
            }
        } else {
            if (xTaskCreate(POWER_MANAGEMENT_task, "power management", 8192, (void *) &GLOBAL_STATE, 10, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Error creating power management task");
            }
            if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
                if (xTaskCreate(FAN_CONTROLLER_task, "fan_controller", 8192, (void *) &GLOBAL_STATE, 10, NULL) != pdPASS) {
                    ESP_LOGE(TAG, "Error creating fan controller task");
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "Critical peripheral initialization failure (%s). Entering degraded mode.",
                 esp_err_to_name(system_init_ret));
    }

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        // Start the API for AxeOS during normal mining boots only.
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
    while (!GLOBAL_STATE.SYSTEM_MODULE.is_connected && !GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        if (GLOBAL_STATE.SYSTEM_MODULE.ap_enabled && setup_ble_grace_ms >= 5000) {
            setup_ble_start(&GLOBAL_STATE);
        }
        setup_ble_grace_ms += 100;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Connected to WiFi: tear down the setup BLE service to free the radio.
    setup_ble_stop();

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active && nvs_config_get_bool(NVS_CONFIG_USE_NTP)) {
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

    if (GLOBAL_STATE.DEVICE_CONFIG.bonanza_bridge) {
        if (!bzm_controller_mining_stack_ready()) {
            ESP_LOGE(TAG, "Bonanza remained safe-off after automatic startup failure");
            system_init_ret = ESP_FAIL;
        } else if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active &&
                   xTaskCreateWithCaps(FAN_CONTROLLER_task, "fan_controller",
                                       8192, (void *) &GLOBAL_STATE, 5, NULL,
                                       MALLOC_CAP_SPIRAM) != pdPASS) {
            /* The bridge is still holding the last safe full-speed command,
             * so mining can remain thermally protected even when dynamic
             * control could not start. Surface the degraded state instead of
             * silently claiming that settings are being applied. */
            ESP_LOGE(TAG, "Bonanza fan controller task could not start; fan remains at 100%%");
            GLOBAL_STATE.SYSTEM_MODULE.hardware_fault = true;
            snprintf(GLOBAL_STATE.SYSTEM_MODULE.hardware_fault_msg,
                     sizeof(GLOBAL_STATE.SYSTEM_MODULE.hardware_fault_msg),
                     "Fan controller task failed; fan held at 100%%");
        }
        if (GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
            GLOBAL_STATE.SELF_TEST_MODULE.system_init_ret = system_init_ret;
            if (xTaskCreateWithCaps(self_test_task, "self_test", 8192, &GLOBAL_STATE, 10,
                                    NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
                (void)bzm_controller_pause();
                ESP_LOGE(TAG, "Self-test task creation failed; Bonanza stopped");
            }
        }
        return;
    }

    if (system_init_ret == ESP_OK) {
        if (asic_initialize(&GLOBAL_STATE, ASIC_INIT_COLD_BOOT, 0) == 0) {
            if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
                return;
            }

            self_test_show_message(&GLOBAL_STATE, GLOBAL_STATE.SYSTEM_MODULE.asic_status);
            system_init_ret = ESP_FAIL;
        } else {
            if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active && xTaskCreate(create_jobs_task, "stratum miner", 8192, (void *) &GLOBAL_STATE, 20, &GLOBAL_STATE.create_jobs_task_handle) != pdPASS) {
                ESP_LOGE(TAG, "Error creating stratum miner task");
            }
            if (xTaskCreateWithCaps(ASIC_result_task, "asic result", 8192,
                                    (void *)&GLOBAL_STATE, 15, NULL,
                                    MALLOC_CAP_SPIRAM) != pdPASS) {
                ESP_LOGE(TAG, "Error creating asic result task");
            }

            if (xTaskCreateWithCaps(hashrate_monitor_task, "hashrate monitor", 8192, (void *) &GLOBAL_STATE, 5, NULL,
                                    MALLOC_CAP_SPIRAM) != pdPASS) {
                ESP_LOGE(TAG, "Error creating hashrate monitor task");
            }
            if (xTaskCreateWithCaps(statistics_task, "statistics", 8192, (void *) &GLOBAL_STATE, 3, NULL, MALLOC_CAP_SPIRAM) !=
                pdPASS) {
                ESP_LOGE(TAG, "Error creating statistics task");
            }
        }
    }

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        if (xTaskCreateWithCaps(stratum_task, "stratum", 16384, (void *) &GLOBAL_STATE, 5, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
            ESP_LOGE(TAG, "Error creating stratum task");
        }
    }

    if (GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        GLOBAL_STATE.SELF_TEST_MODULE.system_init_ret = system_init_ret;
        if (xTaskCreateWithCaps(self_test_task, "self_test", 8192, (void *) &GLOBAL_STATE, 10, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
            ESP_LOGE(TAG, "Error creating self test task");
        }
    }
}
