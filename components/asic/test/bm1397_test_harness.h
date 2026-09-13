#ifndef BM1397_TEST_HARNESS_H
#define BM1397_TEST_HARNESS_H

#include <stddef.h>
#include <stdint.h>
#include "asic_common.h"
#include "bm_job.h"
#include "mining.h"

typedef struct GlobalState GlobalState;
typedef struct bm_job bm_job;

enum { BM1397_HARNESS_RESPONSE_SIZE = 9 };

typedef struct {
    uint8_t (*init)(GlobalState *state);
    bool (*send_work)(GlobalState *state, const bm_job *job, const mining_template_t *work);
    task_result *(*process_work)(GlobalState *state);
} bm1397_harness_driver_t;

typedef struct {
    uint8_t bytes[160];
    size_t length;
} bm1397_harness_packet_t;

extern const bm1397_harness_driver_t bm1397_harness_driver;

GlobalState *bm1397_harness_begin(void);
void bm1397_harness_end(void);
void bm1397_harness_clear_packets(void);
size_t bm1397_harness_packet_count(void);
const bm1397_harness_packet_t *bm1397_harness_packet(size_t index);
bool bm1397_harness_snapshot(uint8_t job_id, mining_template_t *work);
void bm1397_harness_install_job(uint8_t job_id, const mining_template_t *job);
void bm1397_harness_mark_job_valid(uint8_t job_id);
void bm1397_harness_queue_response(
    const uint8_t response[BM1397_HARNESS_RESPONSE_SIZE]);

#endif
