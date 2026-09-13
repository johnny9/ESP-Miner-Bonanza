#include "unity.h"
#include "asic_job_store.h"
#include <string.h>

TEST_CASE("Common work snapshots survive reuse while old generated handles expire", "[asic][common-work]")
{
    asic_job_store_t store;
    TEST_ASSERT_TRUE(asic_job_store_init(&store));
    char job_id[] = "original";
    char extranonce[] = "aabb";
    mining_template_t source = {
        .version = 0x20000000, .ntime = 123,
        .share = {.job_id = job_id, .extranonce2 = extranonce, .work_generation = 7},
    };
    asic_work_handle_t original, replacement;
    TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &source, &original));
    mining_template_t snapshot;
    TEST_ASSERT_TRUE(asic_job_store_snapshot(&store, original, &snapshot));
    memset(job_id, 'x', strlen(job_id));
    memset(extranonce, '0', strlen(extranonce));
    for (unsigned i = 0; i < ASIC_JOB_STORE_CAPACITY; ++i) {
        TEST_ASSERT_TRUE(asic_job_store_store_generated(&store, &source, &replacement));
    }
    TEST_ASSERT_TRUE(original != replacement);
    TEST_ASSERT_FALSE(asic_job_store_contains(&store, original));
    TEST_ASSERT_FALSE(asic_job_store_release(&store, original));
    TEST_ASSERT_TRUE(asic_job_store_contains(&store, replacement));
    TEST_ASSERT_EQUAL_STRING("original", snapshot.share.job_id);
    TEST_ASSERT_EQUAL_STRING("aabb", snapshot.share.extranonce2);
    TEST_ASSERT_TRUE(snapshot.share.work_generation == 7);
    asic_job_store_invalidate_all(&store);
    TEST_ASSERT_FALSE(asic_job_store_contains(&store, replacement));
    TEST_ASSERT_EQUAL_STRING("original", snapshot.share.job_id);
    mining_template_free(&snapshot);
    asic_job_store_destroy(&store);
}
