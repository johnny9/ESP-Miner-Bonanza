#include "hashrate_counter_window.h"

#include <stddef.h>
#include <string.h>

void hashrate_counter_window_reset(hashrate_counter_window_t * window)
{
    if (window != NULL) {
        memset(window, 0, sizeof(*window));
    }
}

bool hashrate_counter_window_update(hashrate_counter_window_t * window, uint32_t value, uint64_t time_us, uint32_t * counter_delta,
                                    uint64_t * duration_us)
{
    if (window == NULL || counter_delta == NULL || duration_us == NULL) {
        return false;
    }

    if (window->count > 0) {
        uint8_t latest = window->next == 0 ? HASHRATE_COUNTER_WINDOW_SNAPSHOTS - 1 : window->next - 1;
        if (time_us <= window->snapshots[latest].time_us) {
            return false;
        }
    }

    uint8_t current = window->next;
    window->snapshots[current].value = value;
    window->snapshots[current].time_us = time_us;
    window->next = (current + 1) % HASHRATE_COUNTER_WINDOW_SNAPSHOTS;
    if (window->count < HASHRATE_COUNTER_WINDOW_SNAPSHOTS) {
        window->count++;
    }

    const hashrate_counter_snapshot_t * baseline = NULL;
    uint64_t baseline_age_us = UINT64_MAX;
    for (uint8_t i = 0; i < window->count; i++) {
        const hashrate_counter_snapshot_t * candidate = &window->snapshots[i];
        if (i == current || candidate->time_us >= time_us) {
            continue;
        }

        uint64_t age_us = time_us - candidate->time_us;
        if (age_us >= HASHRATE_COUNTER_WINDOW_US && age_us < baseline_age_us) {
            baseline = candidate;
            baseline_age_us = age_us;
        }
    }

    if (baseline == NULL) {
        return false;
    }

    *counter_delta = value - baseline->value;
    *duration_us = baseline_age_us;
    return true;
}
