#include "asic_reset_backend.h"

#include <stddef.h>

static asic_reset_backend_t configured_backend;
static const asic_reset_backend_t *active_backend = &ASIC_RESET_GPIO_BACKEND;

esp_err_t asic_reset_configure(const asic_reset_backend_t *backend)
{
    if (backend == NULL) {
        active_backend = &ASIC_RESET_GPIO_BACKEND;
        return ESP_OK;
    }
    if (backend->hold_low == NULL || backend->pulse == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    configured_backend = *backend;
    active_backend = &configured_backend;
    return ESP_OK;
}

esp_err_t asic_reset(uint32_t low_ms, uint32_t release_ms)
{
    return active_backend->pulse(active_backend->context, low_ms, release_ms);
}

esp_err_t asic_hold_reset_low(void)
{
    return active_backend->hold_low(active_backend->context);
}
