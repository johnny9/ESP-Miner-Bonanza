#include "unity.h"
#include "self_test_policy.h"
#include "utils.h"
#include <math.h>

TEST_CASE("self-test deadlines expire at the boundary and on a backwards clock", "[self-test-policy]")
{
    TEST_ASSERT_FALSE(self_test_deadline_expired(100,199,100));
    TEST_ASSERT_TRUE(self_test_deadline_expired(100,200,100));
    TEST_ASSERT_TRUE(self_test_deadline_expired(100,99,100));
    TEST_ASSERT_TRUE(self_test_deadline_expired(100,100,0));
    TEST_ASSERT_FALSE(self_test_deadline_expired(UINT64_MAX-1,UINT64_MAX,2));
}

TEST_CASE("self-test domain evidence rejects stale duplicate invalid and old run samples", "[self-test-policy]")
{
    self_test_domain_average_t average={0};
    self_test_domain_add(&average,(self_test_domain_sample_t){45,1,1,false},45);
    self_test_domain_add(&average,(self_test_domain_sample_t){45,0,1,true},45);
    TEST_ASSERT_EQUAL(0,average.last_sample_time_us);
    self_test_domain_add(&average,(self_test_domain_sample_t){45,1,1,true},45);
    TEST_ASSERT_EQUAL(0,average.sample_count);
    self_test_domain_add(&average,(self_test_domain_sample_t){45,2,1,true},45);
    self_test_domain_add(&average,(self_test_domain_sample_t){999,2,1,true},45);
    self_test_domain_add(&average,(self_test_domain_sample_t){999,1,1,true},45);
    TEST_ASSERT_EQUAL(1,average.sample_count);
    TEST_ASSERT_EQUAL_FLOAT(45,self_test_domain_average(&average));
    const float invalid[]={NAN,-1,136};
    for (unsigned i=0;i<3;i++) self_test_domain_add(&average,
        (self_test_domain_sample_t){invalid[i],3+i,1,true},45);
    TEST_ASSERT_EQUAL(3,average.rejected_sample_count);
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_UNRELIABLE,self_test_domain_status(&average,45));
    self_test_domain_add(&average,(self_test_domain_sample_t){45,1,2,true},45);
    TEST_ASSERT_EQUAL(0,average.sample_count);
    TEST_ASSERT_EQUAL(0,average.rejected_sample_count);
    TEST_ASSERT_EQUAL_FLOAT(0,self_test_domain_average(&average));
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_FAIL,self_test_domain_status(&average,45));
    average.sample_count=UINT32_MAX;
    average.hashrate_sum=123;
    average.rejected_sample_count=UINT32_MAX;
    self_test_domain_add(&average,(self_test_domain_sample_t){45,2,2,true},45);
    self_test_domain_add(&average,(self_test_domain_sample_t){NAN,3,2,true},45);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,average.sample_count);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,average.rejected_sample_count);
    TEST_ASSERT_EQUAL_FLOAT(123,average.hashrate_sum);
}

TEST_CASE("self-test domain judgement keeps inclusive bounds and unreliable classification", "[self-test-policy]")
{
    self_test_domain_average_t average={.sample_count=3};
    average.hashrate_sum=300*(1.0f-0.33f);
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_OK,self_test_domain_status(&average,100));
    average.hashrate_sum=300*(1.0f+0.33f);
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_OK,self_test_domain_status(&average,100));
    average.hashrate_sum=198;
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_FAIL,self_test_domain_status(&average,100));
    average.hashrate_sum=402;
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_FAIL,self_test_domain_status(&average,100));
    average=(self_test_domain_average_t){.sample_count=3,.rejected_sample_count=1,.hashrate_sum=300};
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_UNRELIABLE,self_test_domain_status(&average,100));
    average.sample_count=4;average.hashrate_sum=400;
    TEST_ASSERT_EQUAL(SELF_TEST_DOMAIN_OK,self_test_domain_status(&average,100));
}


TEST_CASE("Local self-test keeps the characterized Bitcoin header", "[self-test-policy]")
{
    asic_job_t job;
    self_test_build_job(&job);
    uint8_t header[80];
    char encoded[161];
    asic_job_header(&job, 0, job.version, header);
    bin2hex(header, sizeof(header), encoded, sizeof(encoded));
    TEST_ASSERT_EQUAL_STRING("040000204595850c738349a3fa5274a511b72ec265a4f23d524305000000000000000000749dbb4faae4227c0c61b8552521df77afd7927074e0b68c351ff511aa11dfdbb52570643aae051700000000", encoded);
    TEST_ASSERT_EQUAL_STRING("self-test", job.job_id);
    TEST_ASSERT_EQUAL_DOUBLE(0, job.pool_diff);
    TEST_ASSERT_TRUE(self_test_in_range(900,1000,.1f));
    TEST_ASSERT_TRUE(self_test_in_range(1100,1000,.1f));
    TEST_ASSERT_FALSE(self_test_in_range(899,1000,.1f));
    TEST_ASSERT_FALSE(self_test_in_range(1101,1000,.1f));
    TEST_ASSERT_FALSE(self_test_in_range(NAN,1000,.1f));
}
