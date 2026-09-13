#pragma once

#include "sv1_client.h"

/* Match the socket receive policy even when transport polling returns zero. */
#define SV1_RECEIVE_TIMEOUT_US (180LL * 1000000LL)

/* Internal clock boundary; the firmware supplies esp_timer_get_time. */
char *sv1_receive_jsonrpc_line_with_clock(esp_transport_handle_t transport,
                                        int64_t (*now_us)(void));
