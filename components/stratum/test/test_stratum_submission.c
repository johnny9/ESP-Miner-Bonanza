#include "unity.h"
#include "asic_share.h"
#include "global_state.h"
#include "stratum_submission.h"
#include "stratum_task.h"
#include "freertos/task.h"
#include <string.h>

static GlobalState state;
static SemaphoreHandle_t entered, release_write, probe_done;
static atomic_uint sends, completed;
static atomic_bool probe_ok, captured_ok;
static asic_share_snapshot_t received;
static int write_result;

/* Link-time network port: hold the real socket mutex across a deliberately
 * blocked write. The production queue, worker, and generation readers run. */
int stratum_submit_share(GlobalState *global, const asic_share_submission_t *share,
                         uint64_t *sent_time_us)
{
    pthread_mutex_lock(&global->transport_mutex);
    atomic_store(&captured_ok, asic_share_snapshot_capture(&received, share));
    atomic_fetch_add(&sends, 1);
    xSemaphoreGive(entered);
    xSemaphoreTake(release_write, portMAX_DELAY);
    *sent_time_us = 2000;
    pthread_mutex_unlock(&global->transport_mutex);
    atomic_fetch_add(&completed, 1);
    return write_result;
}

static asic_share_submission_t fixture_share(uint64_t generation)
{
    static const asic_result_t result = {.timestamp_us = 1000, .nonce = 7};
    return (asic_share_submission_t){.result = &result, .job_id = "42", .extranonce2 = "aabb",
        .extranonce2_len = 2, .nonce = 7, .work_generation = generation};
}

static void probe_result_path(void *context)
{
    (void)context;
    bool ok = stratum_work_is_current(&state, 1) && !stratum_work_is_current(&state, 0);
    /* Already retired entries must be discarded after the stalled write. */
    asic_share_submission_t share = fixture_share(0);
    for (unsigned i = 0; i < STRATUM_SHARE_QUEUE_LENGTH; ++i)
        ok = stratum_queue_share(&state, &share) == 0 && ok;
    ok = stratum_queue_share(&state, &share) < 0 && ok;
    atomic_store(&probe_ok, ok);
    xSemaphoreGive(probe_done);
    vTaskDelete(NULL);
}

TEST_CASE("Result handoff stays bounded while a socket write holds the transport lock", "[stratum][share-queue][qemu-integration]")
{
    memset(&state, 0, sizeof(state));
    atomic_init(&state.stratum_work_generation, 1);
    pthread_mutex_init(&state.transport_mutex, NULL);
    entered = xSemaphoreCreateBinary();
    release_write = xSemaphoreCreateBinary();
    probe_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(entered);
    TEST_ASSERT_NOT_NULL(release_write);
    TEST_ASSERT_NOT_NULL(probe_done);
    TEST_ASSERT_FALSE(stratum_submission_init(NULL));
    TEST_ASSERT_TRUE(stratum_submission_init(&state));
    TEST_ASSERT_FALSE(stratum_submission_init(&state));
    atomic_store(&sends, 0);
    atomic_store(&completed, 0);
    atomic_store(&probe_ok, false);
    write_result = 1;
    TaskHandle_t worker;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(stratum_submission_task, "submit test", 8192, &state, 5, &worker));

    asic_share_submission_t share = fixture_share(1);
    TEST_ASSERT_LESS_THAN(0, stratum_queue_share(NULL, &share));
    TEST_ASSERT_LESS_THAN(0, stratum_queue_share(&state, NULL));
    TEST_ASSERT_EQUAL(0, stratum_queue_share(&state, &share));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_TRUE(atomic_load(&captured_ok));
    TEST_ASSERT_EQUAL_STRING("42", received.job_id);

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(probe_result_path, "result probe", 4096, NULL, 6, NULL));
    bool probe_finished = xSemaphoreTake(probe_done, pdMS_TO_TICKS(1000)) == pdTRUE;
    /* Release the write even if the regression reintroduces a blocking read. */
    xSemaphoreGive(release_write);
    if (!probe_finished) xSemaphoreTake(probe_done, pdMS_TO_TICKS(1000));
    for (unsigned i = 0; i < 100 &&
        (atomic_load(&completed) == 0 || uxQueueMessagesWaiting(state.stratum_share_queue)); ++i)
        vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_TRUE(probe_finished);
    TEST_ASSERT_TRUE(atomic_load(&probe_ok));
    TEST_ASSERT_EQUAL_UINT32(1, state.stratum_share_drops);
    TEST_ASSERT_EQUAL_UINT32(1, atomic_load(&sends));
    TEST_ASSERT_EQUAL_UINT32(0, uxQueueMessagesWaiting(state.stratum_share_queue));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, state.SYSTEM_MODULE.process_time);

    /* Failure does not credit send timing; queued snapshots are independent
     * of the producer's stack and are used by all three protocol paths. */
    for (int protocol = JOB_TYPE_V1; protocol <= JOB_TYPE_SV2_EXTENDED; ++protocol) {
        write_result = -1;
        share = fixture_share(1);
        share.protocol = (mining_job_source_t)protocol;
        char id[] = "owned";
        share.job_id = id;
        TEST_ASSERT_EQUAL(0, stratum_queue_share(&state, &share));
        memset(id, 'x', sizeof(id) - 1);
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(entered, pdMS_TO_TICKS(1000)));
        TEST_ASSERT_EQUAL_STRING("owned", received.job_id);
        TEST_ASSERT_EQUAL(protocol, received.submission.protocol);
        unsigned before = atomic_load(&completed);
        xSemaphoreGive(release_write);
        for (unsigned i = 0; i < 100 && atomic_load(&completed) == before; ++i)
            vTaskDelay(pdMS_TO_TICKS(10));
        TEST_ASSERT_GREATER_THAN(before, atomic_load(&completed));
        TEST_ASSERT_EQUAL_FLOAT(1.0f, state.SYSTEM_MODULE.process_time);
    }
    vTaskDelete(worker);
    vQueueDeleteWithCaps(state.stratum_share_queue);
    state.stratum_share_queue = NULL;
    TEST_ASSERT_LESS_THAN(0, stratum_queue_share(&state, &share));
    vSemaphoreDelete(entered);
    vSemaphoreDelete(release_write);
    vSemaphoreDelete(probe_done);
    pthread_mutex_destroy(&state.transport_mutex);
}

TEST_CASE("Clean jobs and reconnects retire queued generations without changing the public work contract", "[stratum][share-queue][qemu-integration]")
{
    memset(&state, 0, sizeof(state));
    pthread_mutex_init(&state.transport_mutex, NULL);
    atomic_init(&state.stratum_work_generation, UINT64_MAX);
    miner_job_t job = {0};
    stratum_publish_job(&state, &job, 0);
    TEST_ASSERT_TRUE(job.work_generation == UINT64_MAX);
    job.clean_jobs = true;
    stratum_publish_job(&state, &job, 0);
    TEST_ASSERT_TRUE(job.work_generation == 1);
    TEST_ASSERT_FALSE(stratum_work_is_current(&state, UINT64_MAX));
    TEST_ASSERT_TRUE(stratum_work_is_current(&state, 1));
    stratum_invalidate_work(&state);
    TEST_ASSERT_FALSE(stratum_work_is_current(&state, 1));
    TEST_ASSERT_TRUE(stratum_work_is_current(&state, 2));
    pthread_mutex_destroy(&state.transport_mutex);
}
