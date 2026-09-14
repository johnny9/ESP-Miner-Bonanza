#ifndef SELF_TEST_H_
#define SELF_TEST_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

typedef struct GlobalState GlobalState;

typedef struct SelfTestNonceMeasurement
{
    bool is_active;
    uint64_t accepted_count;
    uint64_t rejected_count;
    double hashes;
    pthread_mutex_t lock;
} SelfTestNonceMeasurement;

typedef enum { SELF_TEST_IDLE, SELF_TEST_RUNNING, SELF_TEST_PASSED, SELF_TEST_FAILED, SELF_TEST_CANCELLED } self_test_status_t;

typedef struct SelfTestModule
{
    atomic_bool is_active;
    bool is_factory;
    atomic_bool is_finished;
    SelfTestNonceMeasurement nonce_measurement;
    const char *message;
    char *result;
    char *finished;
    esp_err_t system_init_ret;
    atomic_bool cancel_requested, worker_stop, worker_running, worker_failed;
    atomic_int status;
    atomic_bool start_requested, cleanup_confirmed;
    uint64_t work_generation;
    char message_buffer[128];
    void *domain_averages;
} SelfTestModule;

esp_err_t self_test_init(GlobalState * GLOBAL_STATE);
void self_test_task(void * pvParameters);
void self_test_show_message(GlobalState * GLOBAL_STATE, const char * msg);
void self_test_reset(void);
void self_test_record_nonce(GlobalState * GLOBAL_STATE, double nonce_diff);

void self_test_work_task(void *argument);
bool self_test_start_work(GlobalState *state);
void self_test_stop_work(GlobalState *state);

#endif
