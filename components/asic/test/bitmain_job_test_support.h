#ifndef BITMAIN_JOB_TEST_SUPPORT_H
#define BITMAIN_JOB_TEST_SUPPORT_H
#include "mining.h"
void bitmain_assert_midstate_vector(const mining_template_t *work, unsigned index,
                                    const char *expected_hex);
#endif
