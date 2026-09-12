#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "bzm_bridge.h"
#include "global_state.h"

#define GPIO_ASIC_RESET CONFIG_GPIO_ASIC_RESET

static const char *TAG = "asic_reset";

esp_err_t asic_reset(GlobalState *state, uint32_t low_ms, uint32_t release_ms) {
    if (state != NULL && state->DEVICE_CONFIG.bonanza_bridge) {
        ESP_RETURN_ON_ERROR(BZM_bridge_init(), TAG,
                            "Can't initialize Bonanza bridge");
        return BZM_bridge_pulse_asic_reset();
    }

    esp_rom_gpio_pad_select_gpio(GPIO_ASIC_RESET);
    ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_ASIC_RESET, GPIO_MODE_OUTPUT), TAG, "Can't set GPIO_ASIC_RESET direction");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_RESET, 0), TAG, "Can't set GPIO_ASIC_RESET level to LOW");
    vTaskDelay(pdMS_TO_TICKS(low_ms));
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_RESET, 1), TAG, "Can't set GPIO_ASIC_RESET level to HIGH");
    vTaskDelay(pdMS_TO_TICKS(release_ms));

    return ESP_OK;
}
esp_err_t asic_hold_reset_low(GlobalState *state) {
    if (state != NULL && state->DEVICE_CONFIG.bonanza_bridge) {
        ESP_RETURN_ON_ERROR(BZM_bridge_init(), TAG,
                            "Can't initialize Bonanza bridge");
        return BZM_bridge_set_asic_reset(false);
    }

    esp_rom_gpio_pad_select_gpio(GPIO_ASIC_RESET);
    ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_ASIC_RESET, GPIO_MODE_OUTPUT), TAG, "Can't set GPIO_ASIC_RESET direction");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_RESET, 0), TAG, "Can't set GPIO_ASIC_RESET level to LOW");
    return ESP_OK;
}
