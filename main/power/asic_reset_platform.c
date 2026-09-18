#include "asic_reset_backend.h"

#include "bzm_bridge.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_ASIC_RESET CONFIG_GPIO_ASIC_RESET

static const char *TAG = "asic_reset";

static esp_err_t gpio_hold_low(void *context)
{
    (void)context;
    esp_rom_gpio_pad_select_gpio(GPIO_ASIC_RESET);
    ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_ASIC_RESET, GPIO_MODE_OUTPUT), TAG, "Can't set GPIO_ASIC_RESET direction");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_RESET, 0), TAG, "Can't set GPIO_ASIC_RESET level to LOW");
    return ESP_OK;
}

static esp_err_t gpio_pulse(void *context, uint32_t low_ms, uint32_t release_ms)
{
    ESP_RETURN_ON_ERROR(gpio_hold_low(context), TAG, "Can't hold ASIC reset");
    vTaskDelay(pdMS_TO_TICKS(low_ms));
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_ASIC_RESET, 1), TAG, "Can't set GPIO_ASIC_RESET level to HIGH");
    vTaskDelay(pdMS_TO_TICKS(release_ms));
    return ESP_OK;
}

static esp_err_t bridge_hold_low(void *context)
{
    (void)context;
    ESP_RETURN_ON_ERROR(BZM_bridge_init(), TAG, "Can't initialize Bonanza bridge");
    return BZM_bridge_set_asic_reset(false);
}

static esp_err_t bridge_pulse(void *context, uint32_t low_ms, uint32_t release_ms)
{
    (void)context;
    (void)low_ms;
    (void)release_ms;
    /* The bridge owns the board-specific pulse timing, as before. */
    ESP_RETURN_ON_ERROR(BZM_bridge_init(), TAG, "Can't initialize Bonanza bridge");
    return BZM_bridge_pulse_asic_reset();
}

const asic_reset_backend_t ASIC_RESET_GPIO_BACKEND = {
    .hold_low = gpio_hold_low, .pulse = gpio_pulse,
};
const asic_reset_backend_t ASIC_RESET_BRIDGE_BACKEND = {
    .hold_low = bridge_hold_low, .pulse = bridge_pulse,
};
