#ifndef JOB_TEST_STATE_H
#define JOB_TEST_STATE_H

/* Every QEMU fixture uses the real state layout. Native instances share this
 * explicit view and never exchange state with firmware objects. */
#ifdef ESP_PLATFORM
#include "global_state.h"
#else
#include "stubs/global_state.h"
#include "../../../main/system.h"
#endif

#endif
