#include "self_test_api.h"
#include "http_server.h"
#include "global_state.h"
#include "self_test.h"
#include "bzm_controller.h"
#include "nvs_config.h"
#include "esp_system.h"
#include <string.h>

static esp_err_t authorize(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        return ESP_FAIL;
    }
    return set_cors_headers(req);
}

static esp_err_t get_status(httpd_req_t *req)
{
    if (authorize(req) != ESP_OK) return ESP_FAIL;
    SelfTestModule *test = &((GlobalState *)req->user_ctx)->SELF_TEST_MODULE;
    static const char *names[] = {"idle", "running", "passed", "failed", "cancelled"};
    int status = atomic_load(&test->status);
    cJSON *body = cJSON_CreateObject();
    if (body == NULL) return httpd_resp_send_500(req);
    cJSON_AddStringToObject(body, "status", status >= 0 && status <= SELF_TEST_CANCELLED ? names[status] : "failed");
    cJSON_AddBoolToObject(body, "active", atomic_load(&test->is_active));
    cJSON_AddBoolToObject(body, "cleanupConfirmed", atomic_load(&test->cleanup_confirmed));
    cJSON_AddBoolToObject(body, "workerRunning", atomic_load(&test->worker_running));
    pthread_mutex_lock(&test->nonce_measurement.lock);
    cJSON_AddStringToObject(body, "message", test->message_buffer);
    cJSON_AddNumberToObject(body, "acceptedNonces", test->nonce_measurement.accepted_count);
    cJSON_AddNumberToObject(body, "rejectedNonces", test->nonce_measurement.rejected_count);
    pthread_mutex_unlock(&test->nonce_measurement.lock);
    int capacity = 512;
    esp_err_t result = HTTP_send_json(req, body, &capacity);
    cJSON_Delete(body);
    return result;
}

static esp_err_t post_action(httpd_req_t *req)
{
    if (authorize(req) != ESP_OK) return ESP_FAIL;
    GlobalState *state = req->user_ctx;
    SelfTestModule *test = &state->SELF_TEST_MODULE;
    char data[96];
    if (req->content_len == 0 || req->content_len >= sizeof(data))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected a self-test action");
    size_t used = 0;
    while (used < req->content_len) {
        int count = httpd_req_recv(req, data + used, req->content_len - used);
        if (count <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete action");
        used += count;
    }
    data[used] = 0;
    cJSON *body = cJSON_ParseWithOpts(data, NULL, true);
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(body, "action");
    bool start = cJSON_IsString(action) && strcmp(action->valuestring, "start") == 0;
    bool cancel = cJSON_IsString(action) && strcmp(action->valuestring, "cancel") == 0;
    cJSON_Delete(body);
    if (!start && !cancel)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Action must be start or cancel");
    httpd_resp_set_type(req, "application/json");
    if (cancel) {
        if (atomic_load(&test->is_active)) self_test_reset();
        httpd_resp_set_status(req, "202 Accepted");
        return httpd_resp_sendstr(req, "{\"status\":\"cancel_requested\"}");
    }
    if (atomic_load(&test->is_active) || atomic_exchange(&test->start_requested, true)) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "{\"error\":\"Self-test already active\"}");
    }
    if (!bzm_controller_prepare_restart()) {
        atomic_store(&test->start_requested, false);
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "{\"error\":\"Verified shutdown unavailable\"}");
    }
    /* A one-shot manual request preserves the factory flag and all mining
     * settings. The next boot starts local diagnostics without pool tasks. */
    nvs_config_set_bool(NVS_CONFIG_SELF_TEST_MANUAL, true);
    httpd_resp_set_status(req, "202 Accepted");
    esp_err_t result = httpd_resp_sendstr(req, "{\"status\":\"restarting\"}");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return result;
}

esp_err_t self_test_api_register(httpd_handle_t server, GlobalState *state)
{
    const httpd_uri_t get = {.uri = "/api/system/selftest", .method = HTTP_GET,
        .handler = get_status, .user_ctx = state};
    const httpd_uri_t post = {.uri = "/api/system/selftest", .method = HTTP_POST,
        .handler = post_action, .user_ctx = state};
    esp_err_t result = httpd_register_uri_handler(server, &get);
    return result == ESP_OK ? httpd_register_uri_handler(server, &post) : result;
}
