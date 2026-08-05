#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "button_input.h"
#include "input.h"

#define GPIO_BUTTON_BOOT CONFIG_GPIO_BUTTON_BOOT

#define BUTTON_POLL_MS            10
#define BUTTON_DEBOUNCE_MS        30
#define LONG_PRESS_DURATION_MS  2000

static const char * TAG = "input";

static void (*button_short_clicked_fn)(void) = NULL;
static void (*button_long_pressed_fn)(void) = NULL;
static TaskHandle_t button_task_handle;

static bool button_is_pressed(void)
{
    return gpio_get_level(GPIO_BUTTON_BOOT) == 0;
}

static void button_task(void * pvParameters)
{
    (void) pvParameters;

    TickType_t task_wake_time = xTaskGetTickCount();
    button_input_state_t state;
    button_input_state_init(
        &state, button_is_pressed(), pdTICKS_TO_MS(task_wake_time));

    while (true) {
        uint32_t now_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        button_input_event_t event = button_input_state_update(
            &state, button_is_pressed(), now_ms,
            BUTTON_DEBOUNCE_MS, LONG_PRESS_DURATION_MS);

        if (event == BUTTON_INPUT_EVENT_SHORT_PRESS) {
            if (button_short_clicked_fn != NULL) {
                ESP_LOGI(TAG, "Short button click detected");
                button_short_clicked_fn();
            }
        } else if (event == BUTTON_INPUT_EVENT_LONG_PRESS) {
            if (button_long_pressed_fn != NULL) {
                ESP_LOGI(TAG, "Long button press detected");
                button_long_pressed_fn();
            }
        }

        vTaskDelayUntil(&task_wake_time, pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

esp_err_t input_init(void (*button_short_clicked_cb)(void), void (*button_long_pressed_cb)(void))
{
    if (button_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Install button driver");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BUTTON_BOOT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Error configuring boot button");

    button_short_clicked_fn = button_short_clicked_cb;
    button_long_pressed_fn = button_long_pressed_cb;

    if (xTaskCreate(button_task, "button input", 4096, NULL, 5, &button_task_handle) != pdPASS) {
        button_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create button input task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
