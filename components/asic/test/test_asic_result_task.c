#include "result_task_test_bindings.h"

#include "asic.h"
#include "asic_common.h"
#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "mining.h"
#include "scoreboard.h"
#include "self_test.h"
#include "stratum_task.h"
#include "system.h"
#include "unity.h"

#include <float.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool paused;
    bool invalid;
    bool missing;
    bool self_test;
    bool replace_during_submit;
    unsigned repeated_results;
    double pool_diff;
    int submit_result;
    uint64_t sent_time;
    uint32_t final_ntime;
    uint64_t work_generation;
    mining_job_source_t protocol;
} result_case_t;

static jmp_buf fixture_done;
static GlobalState fixture_state;
static asic_event_t fixture_events[2];
static size_t fixture_event_index;
static result_case_t fixture_case;
static unsigned fixture_delays;
static unsigned fixture_submissions;
static unsigned fixture_scores;
static unsigned fixture_notifications;
static unsigned fixture_self_tests;
static unsigned fixture_registers;
static char fixture_scored_id[32];
static char fixture_submitted_id[32];

/* Independent SHA256d header vectors for the fixture's zero hashes, version
 * 0x20000004, nbits 0x1705dd01 and nonce 7. This catches hashing the saved
 * base time even when submission correctly carries the driver's actual time. */
static void assert_header_difficulty(double difficulty)
{
    double expected = fixture_case.final_ntime == 124
        ? 3.7005742175220721e-09 : 5.1685120699595727e-10;
    TEST_ASSERT_DOUBLE_WITHIN(1e-20, expected, difficulty);
}

void result_task_spy_delay(TickType_t ticks)
{
    TEST_ASSERT_EQUAL_UINT32(pdMS_TO_TICKS(100), ticks);
    fixture_delays++;
    fixture_state.ASIC_initalized = true;
}

asic_event_t *result_task_fake_process_work(GlobalState *state)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    if (fixture_event_index++ == 0) {
        return NULL;
    }
    if (fixture_event_index == 2) {
        return &fixture_events[0];
    }
    if (fixture_event_index <= 3 + fixture_case.repeated_results) {
        return &fixture_events[1];
    }
    longjmp(fixture_done, 1);
}

int result_task_fake_submit_share(GlobalState *state, const asic_job_t *job,
                                  uint32_t nonce, uint32_t version,
                                  uint64_t *sent_time)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    TEST_ASSERT_EQUAL_HEX32(7, nonce);
    TEST_ASSERT_EQUAL_HEX32(0x20000004, version);
    TEST_ASSERT_EQUAL_UINT32(fixture_case.final_ntime ? fixture_case.final_ntime : 123, job->ntime);
    TEST_ASSERT_EQUAL(fixture_case.protocol, job->source_type);
    TEST_ASSERT_NOT_NULL(job->job_id);
    TEST_ASSERT_NOT_NULL(job->extranonce2);
    snprintf(fixture_submitted_id, sizeof(fixture_submitted_id), "%s",
             job->job_id);

    fixture_submissions++;
    if (fixture_case.replace_during_submit) {
        asic_job_store_release(&fixture_state.asic_job_store, 8);
        /* The callback's metadata remains owned after slot invalidation. */
        TEST_ASSERT_EQUAL_STRING("42", job->job_id);
        TEST_ASSERT_EQUAL_STRING("aabb", job->extranonce2);
    }
    *sent_time = fixture_case.sent_time;
    return fixture_case.submit_result;
}

void result_task_spy_record_nonce(GlobalState *state, double difficulty)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    assert_header_difficulty(difficulty);
    fixture_self_tests++;
}

void result_task_spy_notify_found_nonce(GlobalState *state, double difficulty,
                                        uint32_t target)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    assert_header_difficulty(difficulty);
    TEST_ASSERT_EQUAL_HEX32(0x1705dd01, target);
    fixture_notifications++;
}

esp_err_t result_task_spy_scoreboard_add(
    Scoreboard *scoreboard, double difficulty, const char *job_id,
    const char *extranonce, uint32_t ntime, uint32_t nonce,
    uint32_t version_bits)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state.SYSTEM_MODULE.scoreboard, scoreboard);
    assert_header_difficulty(difficulty);
    TEST_ASSERT_EQUAL_UINT32(fixture_case.final_ntime ? fixture_case.final_ntime : 123, ntime);
    TEST_ASSERT_EQUAL_UINT32(7, nonce);
    TEST_ASSERT_EQUAL_UINT32(0, version_bits);
    TEST_ASSERT_EQUAL_STRING("aabb", extranonce);
    snprintf(fixture_scored_id, sizeof(fixture_scored_id), "%s", job_id);
    fixture_scores++;
    return ESP_OK;
}

void result_task_spy_register_read(void *state, register_type_t type,
                                   uint8_t asic_nr, uint32_t value,
                                   uint64_t timestamp)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    TEST_ASSERT_EQUAL(REGISTER_TOTAL_COUNT, type);
    TEST_ASSERT_EQUAL_UINT8(2, asic_nr);
    TEST_ASSERT_EQUAL_UINT32(55, value);
    TEST_ASSERT_TRUE(timestamp == UINT64_C(1000));
    fixture_registers++;
}

bool result_task_fake_work_is_current(GlobalState *state, uint64_t generation)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    return generation == 0;
}

void result_task_spy_local_result(GlobalState *state, uint8_t asic_index,
                                  uint16_t engine_id, bool valid, double difficulty)
{
    TEST_ASSERT_EQUAL_PTR(&fixture_state, state);
    (void)asic_index;
    (void)engine_id;
    TEST_ASSERT_TRUE(valid);
    assert_header_difficulty(difficulty);
}

static void run_result_case(result_case_t test_case)
{
    fixture_case = test_case;
    memset(&fixture_state, 0, sizeof(fixture_state));
    memset(fixture_events, 0, sizeof(fixture_events));
    fixture_state.ASIC_initalized = !fixture_case.paused;
    fixture_state.SELF_TEST_MODULE.is_active = fixture_case.self_test;
    TEST_ASSERT_TRUE(asic_job_store_init(&fixture_state.asic_job_store));
    asic_job_t work = {
        .version = 0x20000004, .ntime = 123, .nbits = 0x1705dd01,
        .pool_diff = fixture_case.pool_diff,
        .source_type = fixture_case.protocol, .job_id = "42",
        .extranonce2 = "aabb", .job_version = 0x20000004,
        .work_generation = fixture_case.work_generation,
    };
    if (!fixture_case.missing) {
        TEST_ASSERT_TRUE(asic_job_store_store_slot(&fixture_state.asic_job_store, 8, &work, NULL));
        if (fixture_case.invalid) asic_job_store_invalidate_all(&fixture_state.asic_job_store);
    }
    fixture_events[0] = (asic_event_t) {
        .type = ASIC_EVENT_REGISTER_RESULT,
        .data.register_result = {.register_type = REGISTER_TOTAL_COUNT,
            .asic_index = 2, .value = 55, .timestamp_us = 1000},
    };
    fixture_events[1] = (asic_event_t) {
        .type = ASIC_EVENT_SHARE_RESULT,
        .data.share = {.work_handle = 8, .nonce = 7,
            .final_ntime = fixture_case.final_ntime ? fixture_case.final_ntime : 123, .final_version = 0x20000004, .timestamp_us = 1000},
    };
    fixture_event_index = 0;
    fixture_delays = 0;
    fixture_submissions = 0;
    fixture_scores = 0;
    fixture_notifications = 0;
    fixture_self_tests = 0;
    fixture_registers = 0;
    fixture_scored_id[0] = '\0';
    fixture_submitted_id[0] = '\0';

    if (setjmp(fixture_done) == 0) {
        ASIC_result_task(&fixture_state);
    }

    TEST_ASSERT_EQUAL_UINT32(1, fixture_registers);
    TEST_ASSERT_EQUAL_UINT32(fixture_case.paused ? 1 : 0, fixture_delays);
    asic_job_store_destroy(&fixture_state.asic_job_store);
}

TEST_CASE("result task keeps owned snapshots through submission for every protocol",
          "[asic][result][ownership][characterization]")
{
    for (int type = JOB_TYPE_V1; type <= JOB_TYPE_SV2_EXTENDED; ++type) {
        run_result_case((result_case_t) {
            .paused = true,
            .pool_diff = 1e-30,
            .protocol = (mining_job_source_t)type,
            .sent_time = 2000,
            .replace_during_submit = false,
        });
        TEST_ASSERT_EQUAL_UINT32(1, fixture_submissions);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_scores);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_notifications);
        TEST_ASSERT_EQUAL_UINT32(0, fixture_self_tests);
        TEST_ASSERT_EQUAL_STRING("42", fixture_submitted_id);
        TEST_ASSERT_EQUAL_STRING("42", fixture_scored_id);
        TEST_ASSERT_EQUAL_FLOAT(1.0f,
                                fixture_state.SYSTEM_MODULE.process_time);
        run_result_case((result_case_t) {
            .pool_diff = 1e-30, .protocol = (mining_job_source_t)type,
            .sent_time = 2000, .replace_during_submit = true,
        });
        TEST_ASSERT_EQUAL_UINT32(1, fixture_submissions);
        TEST_ASSERT_EQUAL_STRING("42", fixture_submitted_id);
        TEST_ASSERT_EQUAL_UINT32(0, fixture_scores);
        TEST_ASSERT_EQUAL_UINT32(0, fixture_notifications);
    }
}

TEST_CASE("result task separates registers and rejects unavailable job slots",
          "[asic][result][job-store][characterization]")
{
    run_result_case((result_case_t) {.invalid = true});
    TEST_ASSERT_EQUAL_UINT32(
        0, fixture_submissions + fixture_scores + fixture_notifications +
               fixture_self_tests);

    run_result_case((result_case_t) {.missing = true});
    TEST_ASSERT_EQUAL_UINT32(
        0, fixture_submissions + fixture_scores + fixture_notifications +
               fixture_self_tests);
}

TEST_CASE("result task preserves thresholds self test and repeated delivery",
          "[asic][result][characterization]")
{
    run_result_case((result_case_t) {.self_test = true});
    TEST_ASSERT_EQUAL_UINT32(1, fixture_self_tests);
    TEST_ASSERT_EQUAL_UINT32(
        0, fixture_submissions + fixture_scores + fixture_notifications);

    const double difficulties[] = {0, DBL_MAX};
    for (size_t index = 0;
         index < sizeof(difficulties) / sizeof(difficulties[0]); ++index) {
        run_result_case(
            (result_case_t) {.pool_diff = difficulties[index]});
        TEST_ASSERT_EQUAL_UINT32(0, fixture_submissions);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_scores);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_notifications);
    }

    run_result_case((result_case_t) {
        .pool_diff = 1e-30,
        .submit_result = -1,
        .sent_time = 2000,
    });
    TEST_ASSERT_EQUAL_UINT32(1, fixture_submissions);
    TEST_ASSERT_EQUAL_UINT32(1, fixture_scores);
    TEST_ASSERT_EQUAL_UINT32(1, fixture_notifications);
    TEST_ASSERT_EQUAL_FLOAT(0, fixture_state.SYSTEM_MODULE.process_time);

    run_result_case((result_case_t) {
        .pool_diff = 1e-30,
        .repeated_results = 1,
    });
    TEST_ASSERT_EQUAL_UINT32(2, fixture_submissions);
    TEST_ASSERT_EQUAL_UINT32(2, fixture_scores);
    TEST_ASSERT_EQUAL_UINT32(2, fixture_notifications);
}

TEST_CASE("Common results preserve actual header time for every pool protocol", "[asic][common-work]")
{
    for (int protocol = JOB_TYPE_V1; protocol <= JOB_TYPE_SV2_EXTENDED; ++protocol) {
        run_result_case((result_case_t) {
            .pool_diff = 1e-30, .protocol = (mining_job_source_t)protocol,
            .final_ntime = 124, .sent_time = 2000,
        });
        TEST_ASSERT_EQUAL_UINT32(1, fixture_submissions);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_scores);
        TEST_ASSERT_EQUAL_UINT32(1, fixture_notifications);
        TEST_ASSERT_EQUAL_FLOAT(1.0f, fixture_state.SYSTEM_MODULE.process_time);
    }
}

TEST_CASE("Common results reject retired pool generations before submission", "[asic][common-work]")
{
    run_result_case((result_case_t) {.pool_diff = 1e-30, .work_generation = 1});
    TEST_ASSERT_EQUAL_UINT32(0, fixture_submissions);
    TEST_ASSERT_EQUAL_UINT32(0, fixture_scores);
    TEST_ASSERT_EQUAL_UINT32(0, fixture_notifications);
}
