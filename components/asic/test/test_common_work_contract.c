#include "unity.h"
#include "asic_job_store.h"
#include "asic_result_handler.h"
#include "mining.h"
#include <string.h>

TEST_CASE("Common work snapshots survive reuse while old generated handles expire", "[asic][common-work]")
{
    asic_job_store_t store;
    TEST_ASSERT_TRUE(asic_job_store_init(&store));
    asic_job_t source = {
        .version = 0x20000000, .ntime = 123,
        .job_id = "original", .extranonce2 = "aabb", .work_generation = 7,
    };
    asic_work_handle_t original, replacement;
    TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &source, &original));
    asic_job_t snapshot;
    TEST_ASSERT_TRUE(asic_job_store_snapshot(&store, original, &snapshot));
    memset(source.job_id, 'x', strlen(source.job_id));
    memset(source.extranonce2, '0', strlen(source.extranonce2));
    for (unsigned i = 0; i < ASIC_JOB_STORE_CAPACITY; ++i) {
        TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &source, &replacement));
    }
    TEST_ASSERT_TRUE(original != replacement);
    TEST_ASSERT_FALSE(asic_job_store_contains(&store, original));
    TEST_ASSERT_FALSE(asic_job_store_release(&store, original));
    TEST_ASSERT_TRUE(asic_job_store_contains(&store, replacement));
    TEST_ASSERT_EQUAL_STRING("original", snapshot.job_id);
    TEST_ASSERT_EQUAL_STRING("aabb", snapshot.extranonce2);
    TEST_ASSERT_TRUE(snapshot.work_generation == 7);
    asic_job_store_invalidate_all(&store);
    TEST_ASSERT_FALSE(asic_job_store_contains(&store, replacement));
    TEST_ASSERT_EQUAL_STRING("original", snapshot.job_id);

    asic_job_store_destroy(&store);
}

TEST_CASE("common header uses exact hash bytes and little endian integers",
          "[asic-job]")
{
    asic_job_t job = {
        .version = 0xffffffff, .ntime = 0x48474645, .nbits = 0x4c4b4a49,
    };
    uint8_t expected[80], actual[80];
    for (unsigned i = 0; i < 80; ++i) expected[i] = (uint8_t)(i + 1);
    for (unsigned i = 0; i < 32; ++i) {
        job.prev_hash[i] = (uint8_t)(i + 5);
        job.merkle_root[i] = (uint8_t)(i + 37);
    }
    asic_job_header(&job, 0x504f4e4d, 0x04030201, actual);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, 80);
}

TEST_CASE("common job builder owns metadata and preserves generic retirement identity",
          "[asic-job][common-work]")
{
    miner_job_t source = {
        .type = JOB_TYPE_SV2_STANDARD, .job_id = "4294967295",
        .version = 0x20000004, .version_mask = 0x1fffe000,
        .ntime = 123, .nbits = 0x1705dd01, .pool_id = 7,
        .pool_diff = 0.125, .clean_jobs = true,
        .work_generation = UINT64_C(0x123456789abcdef0),
    };
    for (unsigned i = 0; i < 32; ++i) {
        source.prev_hash[i] = i;
        source.merkle_root[i] = 32 + i;
    }
    asic_job_t job;
    TEST_ASSERT_TRUE(mining_build_asic_job(&source, 0, 0x20002004, &job));
    TEST_ASSERT_EQUAL_HEX32(0x20002004, job.version);
    TEST_ASSERT_EQUAL_HEX32(source.version, job.job_version);
    TEST_ASSERT_TRUE(source.work_generation == job.work_generation);
    TEST_ASSERT_TRUE(job.clean_jobs);
    TEST_ASSERT_EQUAL_UINT8(source.pool_id, job.pool_id);
    TEST_ASSERT_EQUAL_DOUBLE(source.pool_diff, job.pool_diff);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(source.prev_hash, job.prev_hash, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(source.merkle_root, job.merkle_root, 32);
    asic_job_t copied = job;
    memset(&job, 0, sizeof(job));
    memset(&source, 0, sizeof(source));
    TEST_ASSERT_EQUAL_STRING("4294967295", copied.job_id);
    TEST_ASSERT_EQUAL_STRING("", copied.extranonce2);
    TEST_ASSERT_EQUAL_HEX32(0x20000004, copied.job_version);
}

TEST_CASE("common job builder keeps destination unchanged on rejection and zero uses source version",
          "[asic-job][common-work]")
{
    miner_job_t source = {
        .type = JOB_TYPE_SV2_STANDARD, .job_id = "42", .version = 0x20000004,
    };
    asic_job_t output;
    memset(&output, 0xa5, sizeof(output));
    asic_job_t before = output;
    TEST_ASSERT_FALSE(mining_build_asic_job(NULL, 0, 0, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    memset(source.job_id, 'x', sizeof(source.job_id));
    TEST_ASSERT_FALSE(mining_build_asic_job(&source, 0, 0, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    strcpy(source.job_id, "42");
    source.extranonce2_len = ASIC_JOB_EXTRANONCE2_SIZE + 1;
    TEST_ASSERT_FALSE(mining_build_asic_job(&source, 0, 0, &output));
    TEST_ASSERT_EQUAL_MEMORY(&before, &output, sizeof(output));
    source.extranonce2_len = 0;
    TEST_ASSERT_TRUE(mining_build_asic_job(&source, 0, 0, &output));
    TEST_ASSERT_EQUAL_HEX32(source.version, output.version);
    TEST_ASSERT_EQUAL_HEX32(source.version, output.job_version);
}

TEST_CASE("common results reject unterminated or malformed inline metadata",
          "[asic-job][common-work]")
{
    asic_job_store_t store;
    TEST_ASSERT_TRUE(asic_job_store_init(&store));
    asic_job_t job = {.job_id = "42", .extranonce2 = "aabb"};
    asic_work_handle_t handle;
    asic_result_context_t context = {.job_store = &store, .self_test = true};
    asic_result_callbacks_t callbacks = {0};
    for (unsigned invalid = 0; invalid < 4; ++invalid) {
        asic_job_t malformed = job;
        if (invalid == 0) memset(malformed.job_id, 'x', sizeof(malformed.job_id));
        if (invalid == 1) memset(malformed.extranonce2, '0', sizeof(malformed.extranonce2));
        if (invalid == 2) strcpy(malformed.extranonce2, "abc");
        if (invalid == 3) strcpy(malformed.extranonce2, "xx");
        TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &malformed, &handle));
        asic_result_t result = {.work_handle = handle};
        TEST_ASSERT_EQUAL(ASIC_RESULT_REJECTED_WORK,
                          asic_result_handle(&result, &context, &callbacks));
    }
    TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &job, &handle));
    asic_result_t result = {.work_handle = handle};
    TEST_ASSERT_EQUAL(ASIC_RESULT_RECORDED_SELF_TEST,
                      asic_result_handle(&result, &context, &callbacks));
    asic_job_store_destroy(&store);
}
