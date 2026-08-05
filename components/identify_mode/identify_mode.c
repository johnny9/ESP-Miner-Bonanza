#include "identify_mode.h"

#include <stddef.h>

bool identify_mode_is_active(const identify_mode_t *mode, uint32_t now_ms)
{
    return mode != NULL && mode->deadline_ms != 0U &&
           (int32_t)(mode->deadline_ms - now_ms) > 0;
}

bool identify_mode_toggle(identify_mode_t *mode, uint32_t now_ms,
                          uint32_t duration_ms)
{
    if (mode == NULL || duration_ms == 0U) {
        return false;
    }

    if (identify_mode_is_active(mode, now_ms)) {
        mode->deadline_ms = 0U;
        return false;
    }

    mode->deadline_ms = now_ms + duration_ms;
    if (mode->deadline_ms == 0U) {
        mode->deadline_ms = 1U;
    }
    return true;
}

void identify_mode_cancel(identify_mode_t *mode)
{
    if (mode != NULL) {
        mode->deadline_ms = 0U;
    }
}
