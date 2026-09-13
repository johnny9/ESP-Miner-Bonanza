#ifndef JOB_TEST_STATE_H
#define JOB_TEST_STATE_H

/* Every QEMU fixture uses the real state layout. Native instances share this
 * explicit view and never exchange state with firmware objects. */
#ifdef ESP_PLATFORM
#include "global_state.h"
#else
#include "stubs/global_state.h"
/* The existing system header contains three legacy empty-argument declarations.
 * Keep its declarations real; relax only that header while characterizing tasks. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#endif
#include "../../../main/system.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

#endif
