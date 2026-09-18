#ifndef ASIC_RESET_BACKEND_H_
#define ASIC_RESET_BACKEND_H_

#include "asic_reset.h"

typedef struct {
    void *context;
    esp_err_t (*hold_low)(void *context);
    esp_err_t (*pulse)(void *context, uint32_t low_ms, uint32_t release_ms);
} asic_reset_backend_t;

extern const asic_reset_backend_t ASIC_RESET_GPIO_BACKEND;
extern const asic_reset_backend_t ASIC_RESET_BRIDGE_BACKEND;

/* Configure once after board identification and before reset I/O or tasks.
 * The callbacks/context are copied; NULL restores the default GPIO backend.
 * Invalid backends leave the current selection unchanged. */
esp_err_t asic_reset_configure(const asic_reset_backend_t *backend);

#endif
