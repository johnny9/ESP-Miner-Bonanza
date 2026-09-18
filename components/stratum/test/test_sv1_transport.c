#include "unity.h"
#include "cJSON.h"
#include "sv1_client.h"
#include "sv1_protocol.h"
#include "../sv1_client_internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

typedef struct {
    const char *data;
    size_t length;
    size_t offset;
    size_t max_chunk;
    unsigned int idle_reads;
    unsigned int read_calls;
    int64_t read_elapsed_us;
} mock_transport_data_t;

static int64_t mock_now_us;

static int64_t mock_receive_clock(void)
{
    return mock_now_us;
}

static int mock_transport_read(esp_transport_handle_t transport, char *buffer,
                               int len, int timeout_ms)
{
    mock_transport_data_t *mock =
        (mock_transport_data_t *)esp_transport_get_context_data(transport);
    if (mock != NULL) {
        mock->read_calls++;
        mock_now_us += mock->read_elapsed_us;
        /* Bound regressions that would otherwise spin forever in the test. */
        if (mock->read_elapsed_us > 0 && mock->read_calls > 50) return -1;
        if (mock->idle_reads > 0) {
            mock->idle_reads--;
            return 0;
        }
    }
    if (mock == NULL || mock->offset >= mock->length) {
        return 0;
    }

    size_t remaining = mock->length - mock->offset;
    size_t bytes_to_copy = MIN((size_t)len, remaining);
    if (mock->max_chunk > 0 && bytes_to_copy > mock->max_chunk) {
        bytes_to_copy = mock->max_chunk;
    }

    memcpy(buffer, mock->data + mock->offset, bytes_to_copy);
    mock->offset += bytes_to_copy;
    return (int)bytes_to_copy;
}

static esp_transport_handle_t create_mock_transport(mock_transport_data_t *data)
{
    esp_transport_handle_t transport = esp_transport_init();
    if (transport == NULL ||
        esp_transport_set_context_data(transport, data) != ESP_OK ||
        esp_transport_set_func(transport, NULL, mock_transport_read, NULL,
                               NULL, NULL, NULL, NULL) != ESP_OK) {
        esp_transport_destroy(transport);
        return NULL;
    }
    return transport;
}

TEST_CASE("SV1 silent transport expires after repeated empty reads", "[stratum][qemu-integration]")
{
    mock_transport_data_t mock = {.idle_reads = 100, .read_elapsed_us = 5000000};
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    mock_now_us = 1234567;

    TEST_ASSERT_NULL(sv1_receive_jsonrpc_line_with_clock(transport, mock_receive_clock));
    TEST_ASSERT_EQUAL_UINT32(36, mock.read_calls);
    TEST_ASSERT_TRUE(mock_now_us == 1234567 + SV1_RECEIVE_TIMEOUT_US);
    esp_transport_destroy(transport);
}

TEST_CASE("SV1 short silence preserves a fragmented line", "[stratum][qemu-integration]")
{
    mock_transport_data_t mock = {
        .data = "{}\n", .length = 3, .max_chunk = 1,
        .idle_reads = 3, .read_elapsed_us = 5000000,
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    mock_now_us = 0;

    char *line = sv1_receive_jsonrpc_line_with_clock(transport, mock_receive_clock);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_EQUAL_STRING("{}", line);
    TEST_ASSERT_EQUAL_UINT32(6, mock.read_calls);
    free(line);
    esp_transport_destroy(transport);
}

TEST_CASE("SV1 partial frames cannot extend the deadline or contaminate reconnects", "[stratum][qemu-integration]")
{
    mock_transport_data_t mock = {
        .data = "{}\n", .length = 3, .max_chunk = 1, .read_elapsed_us = 60000000,
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    mock_now_us = 0;

    TEST_ASSERT_NULL(sv1_receive_jsonrpc_line_with_clock(transport, mock_receive_clock));
    TEST_ASSERT_EQUAL_UINT32(3, mock.read_calls);
    mock = (mock_transport_data_t){.data = "[]\n{}\n", .length = 6};
    char *first = sv1_receive_jsonrpc_line_with_clock(transport, mock_receive_clock);
    char *second = sv1_receive_jsonrpc_line_with_clock(transport, mock_receive_clock);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_STRING("[]", first);
    TEST_ASSERT_EQUAL_STRING("{}", second);
    TEST_ASSERT_EQUAL_UINT32(1, mock.read_calls);
    free(first);
    free(second);
    esp_transport_destroy(transport);
}

TEST_CASE("Receive fragmented JSON-RPC line", "[stratum][security][qemu-integration]")
{
    const char *json = "{\"id\":1,\"result\":true,\"error\":null}\n";
    mock_transport_data_t mock = {
        .data = json,
        .length = strlen(json),
        .max_chunk = 3,
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);

    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    char *line = STRATUM_V1_receive_jsonrpc_line(transport);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_EQUAL_STRING("{\"id\":1,\"result\":true,\"error\":null}", line);

    free(line);
    esp_transport_destroy(transport);
}

TEST_CASE("Receive preserves consecutive JSON-RPC lines", "[stratum][security][qemu-integration]")
{
    const char *json =
        "{\"id\":1,\"result\":true}\n"
        "{\"id\":2,\"result\":false}\n";
    mock_transport_data_t mock = {
        .data = json,
        .length = strlen(json),
        .max_chunk = strlen(json),
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);

    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    char *first = STRATUM_V1_receive_jsonrpc_line(transport);
    char *second = STRATUM_V1_receive_jsonrpc_line(transport);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_STRING("{\"id\":1,\"result\":true}", first);
    TEST_ASSERT_EQUAL_STRING("{\"id\":2,\"result\":false}", second);

    free(first);
    free(second);
    esp_transport_destroy(transport);
}

TEST_CASE("Receive rejects oversized JSON-RPC line", "[stratum][security][qemu-integration]")
{
    size_t data_length = STRATUM_V1_MAX_JSON_LINE_SIZE + 2U;
    char *json = malloc(data_length);
    TEST_ASSERT_NOT_NULL(json);
    memset(json, 'a', data_length - 1U);
    json[data_length - 1U] = '\n';

    mock_transport_data_t mock = {
        .data = json,
        .length = data_length,
        .max_chunk = 1024,
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);

    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    TEST_ASSERT_NULL(STRATUM_V1_receive_jsonrpc_line(transport));

    esp_transport_destroy(transport);
    free(json);
}

TEST_CASE("Receive accepts maximum line and preserves the next line", "[stratum][security][qemu-integration]")
{
    const char *next_line = "{}\n";
    size_t data_length = STRATUM_V1_MAX_JSON_LINE_SIZE + 1U + strlen(next_line);
    char *data = malloc(data_length);
    TEST_ASSERT_NOT_NULL(data);
    memset(data, 'a', STRATUM_V1_MAX_JSON_LINE_SIZE);
    data[STRATUM_V1_MAX_JSON_LINE_SIZE] = '\n';
    memcpy(data + STRATUM_V1_MAX_JSON_LINE_SIZE + 1U, next_line, strlen(next_line));

    mock_transport_data_t mock = {
        .data = data,
        .length = data_length,
        .max_chunk = data_length,
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);

    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    char *maximum_line = STRATUM_V1_receive_jsonrpc_line(transport);
    char *second_line = STRATUM_V1_receive_jsonrpc_line(transport);
    TEST_ASSERT_NOT_NULL(maximum_line);
    TEST_ASSERT_EQUAL_size_t(STRATUM_V1_MAX_JSON_LINE_SIZE, strlen(maximum_line));
    TEST_ASSERT_EQUAL_STRING("{}", second_line);

    free(maximum_line);
    free(second_line);
    esp_transport_destroy(transport);
    free(data);
}

TEST_CASE("Receive rejects embedded NUL", "[stratum][security][qemu-integration]")
{
    const char data[] = {'{', '}', '\0', '\n'};
    mock_transport_data_t mock = {
        .data = data,
        .length = sizeof(data),
        .max_chunk = sizeof(data),
    };
    esp_transport_handle_t transport = create_mock_transport(&mock);
    TEST_ASSERT_NOT_NULL(transport);

    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    TEST_ASSERT_NULL(STRATUM_V1_receive_jsonrpc_line(transport));

    esp_transport_destroy(transport);
}

typedef struct {
    bool allow;
    unsigned int calls;
} restart_guard_test_state_t;

static bool test_restart_guard(void *context)
{
    restart_guard_test_state_t *state = context;
    state->calls++;
    return state->allow;
}

TEST_CASE("Stratum fatal restart obeys the installed safety guard",
          "[stratum][restart][qemu-integration]")
{
    restart_guard_test_state_t state = {.allow = false};

    STRATUM_V1_set_restart_guard(NULL, NULL);
    TEST_ASSERT_TRUE(STRATUM_V1_prepare_restart());

    STRATUM_V1_set_restart_guard(test_restart_guard, &state);
    TEST_ASSERT_FALSE(STRATUM_V1_prepare_restart());
    TEST_ASSERT_EQUAL_UINT32(1, state.calls);

    state.allow = true;
    TEST_ASSERT_TRUE(STRATUM_V1_prepare_restart());
    TEST_ASSERT_EQUAL_UINT32(2, state.calls);
    STRATUM_V1_set_restart_guard(NULL, NULL);
}

static int capture_share_write(esp_transport_handle_t transport, const char *buffer,
                               int length, int timeout_ms)
{
    char *output = esp_transport_get_context_data(transport);
    if (length < 0 || length >= 1024) return -1;
    memcpy(output, buffer, length);
    output[length] = '\0';
    return length;
}

static int incomplete_share_write(esp_transport_handle_t transport, const char *buffer,
                                  int length, int timeout_ms)
{
    (void)buffer;
    (void)timeout_ms;
    int mode = *(int *)esp_transport_get_context_data(transport);
    return mode > 0 ? length - 1 : mode;
}

TEST_CASE("Share writes reject timeouts and partial frames without recording a send", "[stratum][qemu-integration]")
{
    esp_transport_handle_t transport = esp_transport_init();
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_func(transport, NULL, NULL, incomplete_share_write,
                                                   NULL, NULL, NULL, NULL));
    for (int mode = -1; mode <= 1; ++mode) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_context_data(transport, &mode));
        uint64_t sent_time = 0;
        TEST_ASSERT_LESS_THAN(0, STRATUM_V1_submit_share(transport, 4, "worker", "job", "",
            0x647025b5, 0x12345678, NULL, &sent_time));
        TEST_ASSERT_TRUE(sent_time == 0);
    }
    esp_transport_destroy(transport);
}

TEST_CASE("Share wire format omits unnegotiated version bits and includes accepted zero bits", "[stratum][qemu-integration]")
{
    char output[1024];
    esp_transport_handle_t transport = esp_transport_init();
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_context_data(transport, output));
    TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_func(transport, NULL, NULL, capture_share_write,
                                                   NULL, NULL, NULL, NULL));
    TEST_ASSERT_TRUE(STRATUM_V1_initialize_buffer());
    uint32_t bits = 0;
    for (int mode = 0; mode < 3; mode++) {
        if (mode == 2) bits = 0x2000;
        TEST_ASSERT_GREATER_THAN(0, STRATUM_V1_submit_share(transport, 4, "worker", "job", "",
            0x647025b5, 0x12345678, mode == 0 ? NULL : &bits, NULL));
        cJSON *json = cJSON_Parse(output);
        TEST_ASSERT_NOT_NULL(json);
        cJSON *params = cJSON_GetObjectItem(json, "params");
        TEST_ASSERT_EQUAL_INT(mode == 0 ? 5 : 6, cJSON_GetArraySize(params));
        TEST_ASSERT_EQUAL_STRING("", cJSON_GetArrayItem(params, 2)->valuestring);
        if (mode != 0) TEST_ASSERT_EQUAL_STRING(mode == 1 ? "00000000" : "00002000",
                                                cJSON_GetArrayItem(params, 5)->valuestring);
        cJSON_Delete(json);
    }
    esp_transport_destroy(transport);
}
