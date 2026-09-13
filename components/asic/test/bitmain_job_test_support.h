#ifndef BITMAIN_JOB_TEST_SUPPORT_H
#define BITMAIN_JOB_TEST_SUPPORT_H
#include "mining.h"
void bitmain_assert_midstate_vector(const asic_job_t *work, unsigned index,
                                    const char *expected_hex);
#endif
