#ifndef HASHRATE_COUNTER_WINDOW_H_
#define HASHRATE_COUNTER_WINDOW_H_

#include <stdbool.h>
#include <stdint.h>

#define HASHRATE_COUNTER_WINDOW_SECONDS 30
#define HASHRATE_COUNTER_WINDOW_US ((uint64_t) HASHRATE_COUNTER_WINDOW_SECONDS * 1000000ULL)
#define HASHRATE_COUNTER_WINDOW_SNAPSHOTS (HASHRATE_COUNTER_WINDOW_SECONDS + 2)

typedef struct
{
    uint32_t value;
    uint64_t time_us;
} hashrate_counter_snapshot_t;

typedef struct hashrate_counter_window
{
    hashrate_counter_snapshot_t snapshots[HASHRATE_COUNTER_WINDOW_SNAPSHOTS];
    uint8_t next;
    uint8_t count;
} hashrate_counter_window_t;

void hashrate_counter_window_reset(hashrate_counter_window_t * window);

bool hashrate_counter_window_update(hashrate_counter_window_t * window, uint32_t value, uint64_t time_us, uint32_t * counter_delta,
                                    uint64_t * duration_us);

#endif /* HASHRATE_COUNTER_WINDOW_H_ */
