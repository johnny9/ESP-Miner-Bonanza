/******************************************************************************
 *  *
 * References:
 *  1. Stratum Protocol - [link](https://reference.cash/mining/stratum-protocol)
 *****************************************************************************/

#include "stratum_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_tcp.h"
#include "esp_crt_bundle.h"
#include "utils.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sys/param.h>

#define TRANSPORT_TIMEOUT_MS 5000
#define BUFFER_SIZE 1024
#define MAX_EXTRANONCE_2_LEN 32
#define JSON_RPC_BUFFER_LIMIT (STRATUM_V1_MAX_JSON_LINE_SIZE + 2U)
#define MIN_POOL_DIFFICULTY 0.0001
#define MAX_POOL_DIFFICULTY 4294967295.0
#define BITCOIN_GENESIS_NTIME 1231006505
#define MAX_ERROR_MSG_LEN 256
static const char * TAG = "stratum_api";

static char * json_rpc_buffer = NULL;
static size_t json_rpc_buffer_size = 0;
static size_t json_rpc_buffer_len = 0;

static RequestTiming *request_timings = NULL;
static stratum_restart_guard_fn restart_guard = NULL;
static void *restart_guard_context = NULL;

void STRATUM_V1_set_restart_guard(stratum_restart_guard_fn guard,
                                  void *context)
{
    restart_guard = guard;
    restart_guard_context = context;
}

bool STRATUM_V1_prepare_restart(void)
{
    return restart_guard == NULL || restart_guard(restart_guard_context);
}


static RequestTiming* get_request_timing(int request_id) {
    if (request_id < 0) return NULL;
    int index = request_id % MAX_REQUEST_IDS;
    return &request_timings[index];
}

float STRATUM_V1_get_response_time_ms(int request_id, int64_t receive_time_us)
{
    if (request_id < 0) return -1.0;
    
    RequestTiming *timing = get_request_timing(request_id);
    if (!timing || !timing->tracking) {
        return -1.0;
    }
    
    float response_time = (receive_time_us - timing->timestamp_us) / 1000.0f;
    timing->tracking = false;
    return response_time;
}

esp_transport_handle_t STRATUM_V1_transport_init(tls_mode tls, const char * cert)
{
    esp_transport_handle_t transport;
    // tls_transport
    if (tls == DISABLED)
    {
        // tcp_transport
        ESP_LOGI(TAG, "TLS disabled, Using TCP transport");
        transport = esp_transport_tcp_init();
    }
    else{
        // tls_transport
        ESP_LOGI(TAG, "Using TLS transport");
        transport = esp_transport_ssl_init();
        if (transport == NULL) {
            ESP_LOGE(TAG, "Failed to initialize SSL transport");
            return NULL;
        }
        switch(tls){
            case BUNDLED_CRT:
                ESP_LOGI(TAG, "Using default cert bundle");
                esp_transport_ssl_crt_bundle_attach(transport, esp_crt_bundle_attach);
                break;
            case CUSTOM_CRT:
                ESP_LOGI(TAG, "Using custom cert");
                if (cert == NULL) {
                    ESP_LOGE(TAG, "Error: no TLS certificate");
                    return NULL;
                }
                esp_transport_ssl_set_cert_data(transport, cert, strlen(cert));
                break;
            default:
                ESP_LOGE(TAG, "Invalid TLS mode");
                esp_transport_destroy(transport);
                return NULL;
        }
    }
    return transport;
}

bool STRATUM_V1_initialize_buffer(void)
{
    // Free any existing buffer (may be non-NULL if a previous V1 task was running)
    free(json_rpc_buffer);
    json_rpc_buffer = NULL;
    json_rpc_buffer_size = 0;
    json_rpc_buffer_len = 0;

    json_rpc_buffer = malloc(BUFFER_SIZE);
    if (json_rpc_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON-RPC buffer");
        return false;
    }
    json_rpc_buffer_size = BUFFER_SIZE;
    json_rpc_buffer[0] = '\0';

    if (request_timings == NULL) {
        // Default allocation may spill into PSRAM and lets the caller recover
        // from failure without invoking the application's capability abort hook.
        request_timings = malloc(sizeof(RequestTiming) * MAX_REQUEST_IDS);
        if (request_timings == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for request_timings");
            free(json_rpc_buffer);
            json_rpc_buffer = NULL;
            json_rpc_buffer_size = 0;
            return false;
        }
    }

    for (int i = 0; i < MAX_REQUEST_IDS; i++) {
        request_timings[i].timestamp_us = 0;
        request_timings[i].tracking = false;
    }

    return true;
}

static bool ensure_json_buffer_capacity(size_t required_size)
{
    if (required_size > JSON_RPC_BUFFER_LIMIT) {
        return false;
    }

    if (required_size <= json_rpc_buffer_size) {
        return true;
    }

    size_t new_size = json_rpc_buffer_size;
    while (new_size < required_size && new_size < JSON_RPC_BUFFER_LIMIT) {
        new_size = MIN(new_size + BUFFER_SIZE, JSON_RPC_BUFFER_LIMIT);
    }

    char *new_buffer = realloc(json_rpc_buffer, new_size);
    if (new_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to grow JSON-RPC receive buffer to %zu bytes", new_size);
        return false;
    }

    json_rpc_buffer = new_buffer;
    json_rpc_buffer_size = new_size;
    return true;
}

char * STRATUM_V1_receive_jsonrpc_line(esp_transport_handle_t transport)
{
    if (json_rpc_buffer == NULL) {
        if (!STRATUM_V1_initialize_buffer()) {
            return NULL;
        }
    }
    char *line = NULL;
    char recv_buffer[BUFFER_SIZE];
    int nbytes;

    char *newline_pos = memchr(json_rpc_buffer, '\n', json_rpc_buffer_len);
    while (newline_pos == NULL) {
        size_t receive_capacity =
            (STRATUM_V1_MAX_JSON_LINE_SIZE + 1U) - json_rpc_buffer_len;
        size_t receive_size = MIN(sizeof(recv_buffer), receive_capacity);
        nbytes = esp_transport_read(transport, recv_buffer, receive_size,
                                    TRANSPORT_TIMEOUT_MS);
        if (nbytes < 0) {
            const char *err_str;
            switch(nbytes) {
                case ERR_TCP_TRANSPORT_NO_MEM:
                    err_str = "No memory available";
                    break;
                case ERR_TCP_TRANSPORT_CONNECTION_FAILED:
                    err_str = "Connection failed";
                    break;
                case ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN:
                    err_str = "Connection closed by peer";
                    break;
                default:
                    err_str = "Unknown error";
                    break;
            }
            ESP_LOGE(TAG, "Error: transport read failed: %s (code: %d)", err_str, nbytes);
            json_rpc_buffer_len = 0;
            json_rpc_buffer[0] = '\0';
            return NULL;
        }
        if (nbytes > 0) {
            if (memchr(recv_buffer, '\0', (size_t)nbytes) != NULL) {
                ESP_LOGE(TAG, "JSON-RPC stream contains an embedded NUL byte");
                json_rpc_buffer_len = 0;
                json_rpc_buffer[0] = '\0';
                return NULL;
            }

            size_t required_size = json_rpc_buffer_len + (size_t)nbytes + 1U;
            if (!ensure_json_buffer_capacity(required_size)) {
                json_rpc_buffer_len = 0;
                json_rpc_buffer[0] = '\0';
                return NULL;
            }

            memcpy(json_rpc_buffer + json_rpc_buffer_len, recv_buffer,
                   (size_t)nbytes);
            json_rpc_buffer_len += (size_t)nbytes;
            json_rpc_buffer[json_rpc_buffer_len] = '\0';
            newline_pos = memchr(json_rpc_buffer, '\n', json_rpc_buffer_len);

            if (newline_pos == NULL &&
                json_rpc_buffer_len > STRATUM_V1_MAX_JSON_LINE_SIZE) {
                ESP_LOGE(TAG, "JSON-RPC line exceeds %u bytes",
                         STRATUM_V1_MAX_JSON_LINE_SIZE);
                json_rpc_buffer_len = 0;
                json_rpc_buffer[0] = '\0';
                return NULL;
            }
        }
    }

    // Extract the line
    if (newline_pos) {
        size_t line_len = (size_t)(newline_pos - json_rpc_buffer);
        line = strndup(json_rpc_buffer, line_len);  // Copy only up to \n
        size_t remaining_len = json_rpc_buffer_len - line_len - 1U;
        if (remaining_len > 0) {
            memmove(json_rpc_buffer, newline_pos + 1, remaining_len);
        }
        json_rpc_buffer_len = remaining_len;
        json_rpc_buffer[json_rpc_buffer_len] = '\0';
    }
    return line;
}

void STRATUM_V1_reset_message(StratumApiV1Message *message)
{
    if (message->error_str) {
        free(message->error_str);
        message->error_str = NULL;
    }
    if (message->extranonce_str) {
        free(message->extranonce_str);
        message->extranonce_str = NULL;
    }
    if (message->show_message) {
        free(message->show_message);
        message->show_message = NULL;
    }
    if (message->version_string) {
        free(message->version_string);
        message->version_string = NULL;
    }
    message->job = NULL;
    message->method = METHOD_UNKNOWN;
    message->message_id = -1;
    message->response_success = false;
    message->new_difficulty = 0.0;
    message->version_mask = 0;
}

static stratum_method parse_method(const cJSON *method_json)
{
    if (!method_json || !cJSON_IsString(method_json)) {
        return STRATUM_RESULT;
    }

    const char *method = method_json->valuestring;
    if (strcmp(method, "mining.notify") == 0) return MINING_NOTIFY;
    if (strcmp(method, "mining.set_difficulty") == 0) return MINING_SET_DIFFICULTY;
    if (strcmp(method, "mining.set_extranonce") == 0) return MINING_SET_EXTRANONCE;
    if (strcmp(method, "mining.set_version_mask") == 0) return MINING_SET_VERSION_MASK;
    if (strcmp(method, "client.reconnect") == 0) return CLIENT_RECONNECT;
    if (strcmp(method, "mining.ping") == 0) return MINING_PING;
    if (strcmp(method, "client.show_message") == 0) return CLIENT_SHOW_MESSAGE;
    if (strcmp(method, "client.get_version") == 0) return CLIENT_GET_VERSION;
    ESP_LOGI(TAG, "Unhandled method: %s", method);
    return METHOD_UNKNOWN;
}

static bool valid_hex(const char *text, size_t length)
{
    if (text == NULL || strlen(text) != length || (length & 1U)) return false;
    for (size_t i = 0; i < length; i++) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

static bool parse_mining_notify(cJSON *json, miner_job_t *job)
{
    if (job == NULL) return false;
    cJSON *params = cJSON_GetObjectItem(json, "params");
    int params_count = cJSON_GetArraySize(params);
    if (!cJSON_IsArray(params) || params_count < 9) return false;

    cJSON *job_id = cJSON_GetArrayItem(params, 0);
    cJSON *prev_hash = cJSON_GetArrayItem(params, 1);
    cJSON *c1 = cJSON_GetArrayItem(params, 2);
    cJSON *c2 = cJSON_GetArrayItem(params, 3);
    cJSON *branches = cJSON_GetArrayItem(params, 4);
    cJSON *version = cJSON_GetArrayItem(params, 5);
    cJSON *nbits = cJSON_GetArrayItem(params, 6);
    cJSON *ntime = cJSON_GetArrayItem(params, 7);
    // Preserve the supported extension layout with clean_jobs last.
    cJSON *clean = cJSON_GetArrayItem(params, params_count - 1);
    if (!cJSON_IsString(job_id) || !cJSON_IsString(prev_hash) ||
        !cJSON_IsString(c1) || !cJSON_IsString(c2) ||
        !cJSON_IsString(version) || !cJSON_IsString(nbits) ||
        !cJSON_IsString(ntime) || !cJSON_IsBool(clean) ||
        !cJSON_IsArray(branches)) return false;

    size_t c1_chars = strlen(c1->valuestring);
    size_t c2_chars = strlen(c2->valuestring);
    size_t count = cJSON_GetArraySize(branches);
    if (job_id->valuestring[0] == '\0' ||
        strlen(job_id->valuestring) >= sizeof(job->job_id) ||
        c1_chars == 0 || c1_chars > 2U * MAX_COINBASE_PREFIX_LEN ||
        c2_chars == 0 || c2_chars > 2U * MAX_COINBASE_SUFFIX_LEN ||
        count > MAX_MERKLE_BRANCHES ||
        !valid_hex(prev_hash->valuestring, 64) ||
        !valid_hex(c1->valuestring, c1_chars) ||
        !valid_hex(c2->valuestring, c2_chars) ||
        !valid_hex(version->valuestring, 8) ||
        !valid_hex(nbits->valuestring, 8) ||
        !valid_hex(ntime->valuestring, 8)) return false;
    for (size_t i = 0; i < count; i++) {
        cJSON *branch = cJSON_GetArrayItem(branches, i);
        if (!cJSON_IsString(branch) || !valid_hex(branch->valuestring, 64)) return false;
    }

    uint32_t timestamp = strtoul(ntime->valuestring, NULL, 16);
    time_t now = time(NULL);
    if (timestamp < BITCOIN_GENESIS_NTIME ||
        (now > 1704067200 && (uint64_t)timestamp > (uint64_t)now + 7200)) return false;

    // Validate every field before touching the destination, including payloads.
    if (!miner_job_ensure_buffers(job) ||
        c1_chars / 2 > job->coinbase_prefix_capacity ||
        c2_chars / 2 > job->coinbase_suffix_capacity) return false;
    miner_job_reset(job);
    job->type = JOB_TYPE_V1;
    strlcpy(job->job_id, job_id->valuestring, sizeof(job->job_id));
    hex2bin(prev_hash->valuestring, job->prev_hash, 32);
    reverse_endianness_per_word(job->prev_hash);
    job->coinbase_prefix_len = c1_chars / 2;
    job->coinbase_suffix_len = c2_chars / 2;
    hex2bin(c1->valuestring, job->coinbase_prefix, job->coinbase_prefix_len);
    hex2bin(c2->valuestring, job->coinbase_suffix, job->coinbase_suffix_len);
    job->merkle_path_count = count;
    for (size_t i = 0; i < count; i++) {
        hex2bin(cJSON_GetArrayItem(branches, i)->valuestring, job->merkle_path[i], 32);
    }
    job->version = strtoul(version->valuestring, NULL, 16);
    job->nbits = strtoul(nbits->valuestring, NULL, 16);
    job->ntime = timestamp;
    job->clean_jobs = cJSON_IsTrue(clean);
    return true;
}

static bool parse_set_difficulty(cJSON *json, StratumApiV1Message *message)
{
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (!params || !cJSON_IsArray(params) || cJSON_GetArraySize(params) == 0) {
        ESP_LOGE(TAG, "Invalid params for set_difficulty");
        return false;
    }
    cJSON *difficulty = cJSON_GetArrayItem(params, 0);
    if (!difficulty || !cJSON_IsNumber(difficulty)) {
        ESP_LOGE(TAG, "Invalid difficulty value in set_difficulty");
        return false;
    }
    double diff_val = difficulty->valuedouble;
    if (isnan(diff_val) || isinf(diff_val) || diff_val < MIN_POOL_DIFFICULTY || diff_val > MAX_POOL_DIFFICULTY) {
        ESP_LOGE(TAG, "Rejecting out-of-range pool difficulty: %f", diff_val);
        return false;
    }
    message->new_difficulty = diff_val;
    ESP_LOGI(TAG, "Set pool difficulty: %.2f", message->new_difficulty);
    return true;
}

static bool parse_set_version_mask(cJSON *json, StratumApiV1Message *message)
{
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (!params || !cJSON_IsArray(params) || cJSON_GetArraySize(params) == 0) {
        ESP_LOGE(TAG, "Invalid params for set_version_mask");
        return false;
    }
    cJSON *mask = cJSON_GetArrayItem(params, 0);
    if (!cJSON_IsString(mask) || !valid_hex(mask->valuestring, 8)) {
        ESP_LOGE(TAG, "Invalid version mask in set_version_mask");
        return false;
    }
    uint32_t raw_mask = (uint32_t)strtoul(mask->valuestring, NULL, 16);
    if ((raw_mask & ~BIP320_VERSION_ROLLING_MASK) != 0) {
        ESP_LOGW(TAG, "Mask 0x%08" PRIx32 " contains non-BIP320 bits; masking to allowed range", raw_mask);
    }
    message->version_mask = raw_mask & BIP320_VERSION_ROLLING_MASK;
    ESP_LOGI(TAG, "Set version mask: %08" PRIx32, message->version_mask);
    return true;
}

static bool parse_extranonce(cJSON *extranonce1, cJSON *extranonce2_size,
                            StratumApiV1Message *message)
{
    if (!cJSON_IsString(extranonce1) || !cJSON_IsNumber(extranonce2_size)) return false;
    size_t length = strlen(extranonce1->valuestring);
    double size = extranonce2_size->valuedouble;
    if (length > 64 || !valid_hex(extranonce1->valuestring, length) ||
        !isfinite(size) || size < 0 || size > MAX_EXTRANONCE_2_LEN ||
        floor(size) != size) return false;
    char *copy = strdup(extranonce1->valuestring);
    if (copy == NULL) return false;
    free(message->extranonce_str);
    message->extranonce_str = copy;
    message->extranonce_2_len = (int)size;
    return true;
}

static bool parse_set_extranonce(cJSON *json, StratumApiV1Message *message)
{
    cJSON *params = cJSON_GetObjectItem(json, "params");
    return cJSON_IsArray(params) && cJSON_GetArraySize(params) >= 2 &&
        parse_extranonce(cJSON_GetArrayItem(params, 0), cJSON_GetArrayItem(params, 1), message);
}

static bool parse_show_message(cJSON *json, StratumApiV1Message *message)
{
    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (!params || !cJSON_IsArray(params) || cJSON_GetArraySize(params) == 0) {
        ESP_LOGE(TAG, "Invalid params for show_message");
        return false;
    }
    cJSON *msg = cJSON_GetArrayItem(params, 0);
    if (!msg || !cJSON_IsString(msg)) {
        ESP_LOGE(TAG, "Invalid message in show_message");
        return false;
    }
    if (message->show_message) free(message->show_message);
    message->show_message = strndup(msg->valuestring, MAX_POOL_MESSAGE_LEN);
    
    ESP_LOGI(TAG, "Pool message: %s", message->show_message);
    return true;
}

static bool parse_get_version(cJSON *json, StratumApiV1Message *message)
{
    if (message->version_string) free(message->version_string);
    message->version_string = strdup("unknown");
    ESP_LOGI(TAG, "Get version requested");
    return true;
}

static bool parse_subscribe_result(cJSON *json, StratumApiV1Message *message)
{
    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!cJSON_IsArray(cJSON_GetArrayItem(result, 0)) ||
        !parse_extranonce(cJSON_GetArrayItem(result, 1), cJSON_GetArrayItem(result, 2), message)) return false;
    message->response_success = true;
    return true;
}

static bool parse_configure_result(cJSON *json, StratumApiV1Message *message)
{
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *rolling = cJSON_GetObjectItem(result, "version-rolling");
    cJSON *mask = cJSON_GetObjectItem(result, "version-rolling.mask");
    if (cJSON_IsFalse(rolling) || cJSON_IsString(rolling)) {
        message->response_success = false;
        return true;
    }
    if (!cJSON_IsTrue(rolling) || !cJSON_IsString(mask) ||
        !valid_hex(mask->valuestring, 8)) return false;
    message->version_mask = strtoul(mask->valuestring, NULL, 16) & BIP320_VERSION_ROLLING_MASK;
    message->response_success = true;
    return true;
}

bool STRATUM_V1_apply_version_mask(sv1_conn_t *conn,
                                  const StratumApiV1Message *message,
                                  uint32_t supported_mask)
{
    if (conn == NULL || message == NULL) return false;
    if (message->method == STRATUM_RESULT_CONFIGURE) {
        if (message->message_id != conn->configure_uid) return false;
        conn->version_rolling_enabled = message->response_success;
    } else if (message->method != MINING_SET_VERSION_MASK || !conn->version_rolling_enabled) {
        return false;
    }
    conn->version_mask = conn->version_rolling_enabled ? message->version_mask & supported_mask : 0;
    return true;
}

static bool parse_result(cJSON *json, StratumApiV1Message *message)
{
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *error = cJSON_GetObjectItem(json, "error");
    cJSON *reject_reason = cJSON_GetObjectItem(json, "reject-reason");

    message->method = STRATUM_RESULT;

    // Handle error array format: [code, message, extra]
    if (error && cJSON_IsArray(error) && cJSON_GetArraySize(error) >= 2) {
        cJSON *error_msg = cJSON_GetArrayItem(error, 1);
        if (cJSON_IsString(error_msg)) {
            message->response_success = false;
            if (message->error_str) free(message->error_str);
            message->error_str = strndup(error_msg->valuestring, MAX_ERROR_MSG_LEN);
            ESP_LOGD(TAG, "Result failed: %s", message->error_str);
            return true;
        }
    } else if (error && cJSON_IsString(error)) {
        message->response_success = false;
        if (message->error_str) free(message->error_str);
        message->error_str = strndup(error->valuestring, MAX_ERROR_MSG_LEN);
        ESP_LOGD(TAG, "Result failed: %s", message->error_str);
        return true;
    } else if (error && cJSON_IsObject(error)) {
        cJSON *error_msg = cJSON_GetObjectItem(error, "message");
        if (error_msg && cJSON_IsString(error_msg)) {
            message->response_success = false;
            if (message->error_str) free(message->error_str);
            message->error_str = strndup(error_msg->valuestring, MAX_ERROR_MSG_LEN);
            ESP_LOGD(TAG, "Result failed: %s", message->error_str);
            return true;
        }
    }

    // Handle null result or non-null error
    if ((!result || cJSON_IsNull(result)) && (error && !cJSON_IsNull(error))) {
        message->response_success = false;
        if (message->error_str) free(message->error_str);
        message->error_str = (reject_reason && cJSON_IsString(reject_reason))
            ? strndup(reject_reason->valuestring, MAX_ERROR_MSG_LEN)
            : strdup("unknown");
        ESP_LOGD(TAG, "Result failed: %s", message->error_str);
        return true;
    }

    // Handle boolean result
    if (cJSON_IsBool(result)) {
        message->response_success = cJSON_IsTrue(result);
        if (!message->response_success) {
            if (message->error_str) free(message->error_str);
            message->error_str = (reject_reason && cJSON_IsString(reject_reason))
                ? strndup(reject_reason->valuestring, MAX_ERROR_MSG_LEN)
                : strdup("unknown");
            ESP_LOGD(TAG, "Result failed: %s", message->error_str);
        } else {
            ESP_LOGD(TAG, "Result success");
        }
        return true;
    }

    // Handle subscribe result
    if (cJSON_IsArray(result) && cJSON_GetArraySize(result) >= 3) {
        message->method = STRATUM_RESULT_SUBSCRIBE;
        return parse_subscribe_result(json, message);
    }

    // Handle configure result
    if (cJSON_IsObject(result) && cJSON_GetObjectItem(result, "version-rolling")) {
        message->method = STRATUM_RESULT_CONFIGURE;
        return parse_configure_result(json, message);
    }

    ESP_LOGI(TAG, "Unhandled result format");
    return false;
}

bool STRATUM_V1_parse(StratumApiV1Message *message, const char *stratum_json, miner_job_t *job)
{
    if (message == NULL || stratum_json == NULL) {
        return false;
    }

    STRATUM_V1_reset_message(message);
    message->job = job;

    size_t json_length = strnlen(stratum_json, STRATUM_V1_MAX_JSON_LINE_SIZE + 1U);
    if (json_length > STRATUM_V1_MAX_JSON_LINE_SIZE) {
        ESP_LOGE(TAG, "JSON-RPC message exceeds %u bytes", STRATUM_V1_MAX_JSON_LINE_SIZE);
        return false;
    }

    ESP_LOGD(TAG, "rx: %.*s%s", (int)MIN(json_length, 512U), stratum_json,
             json_length > 512U ? "..." : "");

    cJSON *json = cJSON_ParseWithOpts(stratum_json, NULL, true);
    if (!cJSON_IsObject(json)) {
        ESP_LOGE(TAG, "JSON-RPC message is not a valid JSON object: %s", stratum_json);
        message->method = METHOD_UNKNOWN;
        cJSON_Delete(json);
        return false;
    }

    // Parse message ID
    cJSON *id_json = cJSON_GetObjectItem(json, "id");
    if (id_json && !cJSON_IsNull(id_json)) {
        if (!cJSON_IsNumber(id_json) || id_json->valuedouble < 0 ||
            id_json->valuedouble > INT_MAX ||
            id_json->valuedouble != (double)id_json->valueint) {
            ESP_LOGE(TAG, "Invalid JSON-RPC message id");
            cJSON_Delete(json);
            return false;
        }
        message->message_id = id_json->valueint;
    }

    // Parse method or result
    cJSON *method_json = cJSON_GetObjectItem(json, "method");
    message->method = parse_method(method_json);

    bool result = false;
    // Handle requests or results
    switch (message->method) {
        case STRATUM_RESULT:
            result = parse_result(json, message);
            break;
        case MINING_NOTIFY:
            result = parse_mining_notify(json, job);
            break;
        case MINING_SET_DIFFICULTY:
            result = parse_set_difficulty(json, message);
            break;
        case MINING_SET_VERSION_MASK:
            result = parse_set_version_mask(json, message);
            break;
        case MINING_SET_EXTRANONCE:
            result = parse_set_extranonce(json, message);
            break;
        case CLIENT_RECONNECT:
            ESP_LOGI(TAG, "Received client.reconnect");
            result = true;
            break;
        case MINING_PING:
            ESP_LOGI(TAG, "Received mining.ping");
            result = true;
            break;
        case CLIENT_SHOW_MESSAGE:
            result = parse_show_message(json, message);
            break;
        case CLIENT_GET_VERSION:
            result = parse_get_version(json, message);
            break;
        case METHOD_UNKNOWN:
            break;
        default:
            ESP_LOGI(TAG, "No handler for method: %d", message->method);
            break;
    }

    cJSON_Delete(json);
    return result;
}



static void stamp_tx(int request_id, uint64_t timestamp_us)
{
    if (request_id >= 1) {
        RequestTiming *timing = get_request_timing(request_id);
        if (timing) {
            timing->timestamp_us = timestamp_us;
            timing->tracking = true;
        }
    }
}

static void debug_stratum_tx(const char * msg)
{
    char *newline = strchr(msg, '\n');
    if (newline) {
        ESP_LOGD(TAG, "tx: %.*s", (int)(newline - msg), msg);
    } else {
        ESP_LOGD(TAG, "tx: %s", msg);
    }
}

int STRATUM_V1_subscribe(esp_transport_handle_t transport, int send_uid, const char * model)
{
    // Subscribe
    char subscribe_msg[BUFFER_SIZE];
    const esp_app_desc_t *app_desc = esp_app_get_description();
    const char *version = app_desc->version;	
    snprintf(subscribe_msg, sizeof(subscribe_msg),
        "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"bitaxe/%s/%s\"]}\n",
        send_uid, model, version);
    debug_stratum_tx(subscribe_msg);

    return esp_transport_write(transport, subscribe_msg, strlen(subscribe_msg), TRANSPORT_TIMEOUT_MS);
}

int STRATUM_V1_suggest_difficulty(esp_transport_handle_t transport, int send_uid, uint32_t difficulty)
{
    char difficulty_msg[BUFFER_SIZE];
    snprintf(difficulty_msg, sizeof(difficulty_msg),
        "{\"id\":%d,\"method\":\"mining.suggest_difficulty\",\"params\":[%" PRIu32 "]}\n",
        send_uid, difficulty);
    debug_stratum_tx(difficulty_msg);

    return esp_transport_write(transport, difficulty_msg, strlen(difficulty_msg), TRANSPORT_TIMEOUT_MS);
}

int STRATUM_V1_extranonce_subscribe(esp_transport_handle_t transport, int send_uid)
{
    char extranonce_msg[BUFFER_SIZE];
    snprintf(extranonce_msg, sizeof(extranonce_msg),
        "{\"id\":%d,\"method\":\"mining.extranonce.subscribe\",\"params\":[]}\n",
        send_uid);
    debug_stratum_tx(extranonce_msg);

    return esp_transport_write(transport, extranonce_msg, strlen(extranonce_msg), TRANSPORT_TIMEOUT_MS);
}

int STRATUM_V1_authorize(esp_transport_handle_t transport, int send_uid, const char * username, const char * pass)
{
    char authorize_msg[BUFFER_SIZE];
    snprintf(authorize_msg, sizeof(authorize_msg),
        "{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n",
        send_uid, username, pass);
    debug_stratum_tx(authorize_msg);

    return esp_transport_write(transport, authorize_msg, strlen(authorize_msg), TRANSPORT_TIMEOUT_MS);
}

int STRATUM_V1_pong(esp_transport_handle_t transport, int message_id)
{
    char pong_msg[BUFFER_SIZE];
    snprintf(pong_msg, sizeof(pong_msg),
        "{\"id\":%d,\"method\":\"pong\",\"params\":[]}\n",
        message_id);
    debug_stratum_tx(pong_msg);
    
    return esp_transport_write(transport, pong_msg, strlen(pong_msg), TRANSPORT_TIMEOUT_MS);
}

int STRATUM_V1_send_version(esp_transport_handle_t transport, int message_id)
{
    char version_msg[BUFFER_SIZE];
    const esp_app_desc_t *app_desc = esp_app_get_description();
    const char *version = app_desc->version;
    snprintf(version_msg, sizeof(version_msg),
        "{\"id\":%d,\"result\":\"%s\",\"error\":null}\n",
        message_id, version);
    debug_stratum_tx(version_msg);
    
    return esp_transport_write(transport, version_msg, strlen(version_msg), TRANSPORT_TIMEOUT_MS);
}

/// @param transport Transport to write to
/// @param send_uid Message ID
/// @param username The client’s user name.
/// @param job_id The job ID for the work being submitted.
/// @param extranonce_2 The hex-encoded value of extra nonce 2.
/// @param ntime The hex-encoded time value use in the block header.
/// @param nonce The hex-encoded nonce value to use in the block header.
/// @param version_bits Negotiated BIP310 bits, or NULL for the five-parameter base protocol.
/// @param out_sent_time_us Pointer to store the time when the share was sent.
int STRATUM_V1_submit_share(esp_transport_handle_t transport, int send_uid, const char * username, const char * job_id,
                            const char * extranonce_2, const uint32_t ntime,
                            const uint32_t nonce, const uint32_t *version_bits, uint64_t *out_sent_time_us)
{
    char submit_msg[BUFFER_SIZE];
    char version_param[16] = "";
    if (version_bits != NULL) {
        snprintf(version_param, sizeof(version_param), ",\"%08" PRIx32 "\"", *version_bits);
    }
    int size = snprintf(submit_msg, sizeof(submit_msg),
        "{\"id\":%d,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%08lx\",\"%08lx\"%s]}\n",
        send_uid, username, job_id, extranonce_2, ntime, nonce, version_param);
    if (size < 0 || size >= sizeof(submit_msg)) return -1;

    int ret = esp_transport_write(transport, submit_msg, strlen(submit_msg), TRANSPORT_TIMEOUT_MS);

    uint64_t now = esp_timer_get_time();
    if (out_sent_time_us) {
        *out_sent_time_us = now;
    }

    debug_stratum_tx(submit_msg);
    
    stamp_tx(send_uid, now);

    return ret;
}

int STRATUM_V1_configure_version_rolling(esp_transport_handle_t transport, int send_uid, uint32_t version_mask)
{
    char configure_msg[BUFFER_SIZE];
    snprintf(configure_msg, sizeof(configure_msg),
        "{\"id\":%d,\"method\":\"mining.configure\",\"params\":[[\"version-rolling\"],{\"version-rolling.mask\":\"%08lx\",\"version-rolling.min-bit-count\":0}]}\n",
        send_uid, version_mask);
    debug_stratum_tx(configure_msg);

    return esp_transport_write(transport, configure_msg, strlen(configure_msg), TRANSPORT_TIMEOUT_MS);
}

stratum_protocol_t stratum_protocol_from_string(const char *s)
{
    if (!s) return STRATUM_PROTOCOL_UNKNOWN;
    if (strcmp(s, STRATUM_V1) == 0) return STRATUM_PROTOCOL_V1;
    if (strcmp(s, STRATUM_V2) == 0) return STRATUM_PROTOCOL_V2;
    return STRATUM_PROTOCOL_UNKNOWN;
}

const char *stratum_protocol_to_string(stratum_protocol_t p)
{
    switch (p) {
        case STRATUM_PROTOCOL_V1: return STRATUM_V1;
        case STRATUM_PROTOCOL_V2: return STRATUM_V2;
        default: return "unknown";
    }
}
