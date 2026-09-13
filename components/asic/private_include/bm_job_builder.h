#ifndef BM_JOB_BUILDER_H_
#define BM_JOB_BUILDER_H_

#include <stdbool.h>
#include <stdint.h>

#include "bm_job.h"
#include "asic_job.h"

// Convert header-byte-order work into Bitmain packet material, without metadata.
bool bm_job_build_from_asic_job(const asic_job_t *source, bm_job *destination);

#endif // BM_JOB_BUILDER_H_
