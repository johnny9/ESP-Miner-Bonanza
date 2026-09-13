#ifndef SV1_CLIENT_H_
#define SV1_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

#include <esp_transport.h>

#include "sv1_protocol.h"

#define MAX_REQUEST_IDS 1024

typedef enum
{
    DISABLED = 0,
    BUNDLED_CRT = 1,
    CUSTOM_CRT = 2,
} tls_mode;

esp_transport_handle_t STRATUM_V1_transport_init(tls_mode tls, const char *cert);

bool STRATUM_V1_initialize_buffer(void);

char *STRATUM_V1_receive_jsonrpc_line(esp_transport_handle_t transport);

int STRATUM_V1_subscribe(esp_transport_handle_t transport, int send_uid, const char *model);

int STRATUM_V1_authorize(esp_transport_handle_t transport, int send_uid, const char *username, const char *pass);

int STRATUM_V1_configure_version_rolling(esp_transport_handle_t transport, int send_uid, uint32_t version_mask);

int STRATUM_V1_pong(esp_transport_handle_t transport, int message_id);

int STRATUM_V1_send_version(esp_transport_handle_t transport, int message_id);

int STRATUM_V1_suggest_difficulty(esp_transport_handle_t transport, int send_uid, uint32_t difficulty);

int STRATUM_V1_extranonce_subscribe(esp_transport_handle_t transport, int send_uid);

int STRATUM_V1_submit_share(esp_transport_handle_t transport, int send_uid, const char *username, const char *job_id,
                            const char *extranonce_2, const uint32_t ntime, const uint32_t nonce,
                            const uint32_t *version_bits, uint64_t *out_sent_time_us);

float STRATUM_V1_get_response_time_ms(int request_id, int64_t receive_time_us);

typedef bool (*stratum_restart_guard_fn)(void *context);
void STRATUM_V1_set_restart_guard(stratum_restart_guard_fn guard, void *context);
bool STRATUM_V1_prepare_restart(void);

#endif /* SV1_CLIENT_H_ */
