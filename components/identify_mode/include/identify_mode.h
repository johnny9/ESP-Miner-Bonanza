#ifndef IDENTIFY_MODE_H_
#define IDENTIFY_MODE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t deadline_ms;
} identify_mode_t;

bool identify_mode_is_active(const identify_mode_t *mode, uint32_t now_ms);
bool identify_mode_toggle(identify_mode_t *mode, uint32_t now_ms,
                          uint32_t duration_ms);
void identify_mode_cancel(identify_mode_t *mode);

#endif /* IDENTIFY_MODE_H_ */
