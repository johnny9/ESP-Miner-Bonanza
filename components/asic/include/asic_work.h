#ifndef ASIC_WORK_H
#define ASIC_WORK_H

#include "asic_result.h"
#include "asic_job.h"

typedef struct {
    asic_work_handle_t handle;
    const asic_job_t *template;
} asic_work_t;

#endif // ASIC_WORK_H
