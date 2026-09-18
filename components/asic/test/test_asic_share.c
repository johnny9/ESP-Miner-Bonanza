#include "asic_share.h"
#include "unity.h"

#include <string.h>

TEST_CASE("Queued shares own result metadata after buffers are reused", "[asic][share-queue]")
{
    for (int protocol = JOB_TYPE_V1; protocol <= JOB_TYPE_SV2_EXTENDED; ++protocol) {
        asic_result_t result = {
            .nonce = 7, .timestamp_us = 123456, .work_handle = 99,
            .job_valid = true,
            .job = {.version = 0x20000000, .job_id = "123", .extranonce2 = "aabb",
                    .source_type = (mining_job_source_t)protocol},
            .context = {.work_generation = UINT64_C(0x123456789), .pool_work = true},
        };
        char job_id[ASIC_JOB_ID_LEN] = "123";
        char extranonce[ASIC_JOB_EXTRANONCE2_HEX_SIZE] = "aabb";
        asic_share_submission_t share = {
            .result = &result, .username = "unused connection identity",
            .job_id = job_id, .extranonce2 = extranonce,
            .extranonce2_bin = {0xaa, 0xbb}, .extranonce2_len = 2,
            .numeric_job_id = 123, .nonce = 7, .ntime = 456,
            .final_version = 0x20002000, .version_bits = 0x2000,
            .job_version = 0x20000000, .pool_id = 3, .work_generation = UINT64_C(0x123456789),
            .protocol = (mining_job_source_t)protocol,
        };
        asic_share_snapshot_t captured;
        TEST_ASSERT_TRUE(asic_share_snapshot_capture(&captured, &share));
        TEST_ASSERT_NULL(captured.submission.result);
        TEST_ASSERT_NULL(captured.submission.username);
        TEST_ASSERT_NULL(captured.submission.job_id);
        TEST_ASSERT_NULL(captured.submission.extranonce2);
        asic_share_snapshot_t relocated = captured;
        memset(&captured, 0, sizeof(captured));
        memset(&result, 0, sizeof(result));
        memset(job_id, 0, sizeof(job_id));
        memset(extranonce, 0, sizeof(extranonce));
        share = asic_share_snapshot_view(&relocated);
        TEST_ASSERT_EQUAL_STRING("123", share.job_id);
        TEST_ASSERT_EQUAL_STRING("aabb", share.extranonce2);
        TEST_ASSERT_TRUE(share.result->timestamp_us == 123456);
        TEST_ASSERT_TRUE(share.result->work_handle == 99);
        TEST_ASSERT_TRUE(share.result->job_valid);
        TEST_ASSERT_EQUAL_STRING("123", share.result->job.job_id);
        TEST_ASSERT_EQUAL_STRING("aabb", share.result->job.extranonce2);
        TEST_ASSERT_EQUAL_HEX32(0x20000000, share.result->job.version);
        TEST_ASSERT_EQUAL(protocol, share.result->job.source_type);
        TEST_ASSERT_TRUE(share.result->context.work_generation == UINT64_C(0x123456789));
        TEST_ASSERT_TRUE(share.result->context.pool_work);
        TEST_ASSERT_EQUAL_UINT32(7, share.nonce);
        TEST_ASSERT_EQUAL_UINT32(456, share.ntime);
        TEST_ASSERT_EQUAL_HEX32(0x20002000, share.final_version);
        TEST_ASSERT_EQUAL_HEX32(0x2000, share.version_bits);
        TEST_ASSERT_EQUAL_HEX32(0x20000000, share.job_version);
        TEST_ASSERT_EQUAL_UINT8(3, share.pool_id);
        TEST_ASSERT_TRUE(share.work_generation == UINT64_C(0x123456789));
        TEST_ASSERT_EQUAL(protocol, share.protocol);
        const uint8_t expected[] = {0xaa, 0xbb};
        TEST_ASSERT_EQUAL_MEMORY(expected, share.extranonce2_bin, sizeof(expected));
    }
}

TEST_CASE("Share snapshots bound metadata and reject missing ownership", "[asic][share-queue]")
{
    asic_share_snapshot_t snapshot;
    asic_result_t result = {0};
    char job_id[ASIC_JOB_ID_LEN + 1];
    char extranonce[ASIC_JOB_EXTRANONCE2_HEX_SIZE + 1];
    memset(job_id, 'a', sizeof(job_id));
    memset(extranonce, 'b', sizeof(extranonce));
    job_id[sizeof(job_id) - 1] = 0;
    extranonce[sizeof(extranonce) - 1] = 0;
    asic_share_submission_t share = {.result = &result, .job_id = job_id, .extranonce2 = ""};
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    job_id[sizeof(job_id) - 2] = 0;
    TEST_ASSERT_TRUE(asic_share_snapshot_capture(&snapshot, &share));
    share.extranonce2 = extranonce;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    extranonce[sizeof(extranonce) - 2] = 0;
    share.extranonce2_len = ASIC_JOB_EXTRANONCE2_SIZE;
    TEST_ASSERT_TRUE(asic_share_snapshot_capture(&snapshot, &share));
    share.extranonce2_len++;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    share.extranonce2_len = 0;
    share.protocol = (mining_job_source_t)99;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    share.protocol = JOB_TYPE_V1;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(NULL, &share));
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, NULL));
    share.result = NULL;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    share.result = &result;
    share.job_id = NULL;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
    share.job_id = "";
    share.extranonce2 = NULL;
    TEST_ASSERT_FALSE(asic_share_snapshot_capture(&snapshot, &share));
}
