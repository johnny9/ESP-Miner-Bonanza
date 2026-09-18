#include "unity.h"
#include "sv2_noise.h"

typedef struct { int write_result; unsigned writes, reads; } transport_probe_t;

static int incomplete_write(esp_transport_handle_t transport, const char *buffer,
                             int length, int timeout_ms)
{
    (void)buffer;
    (void)length;
    (void)timeout_ms;
    transport_probe_t *probe = esp_transport_get_context_data(transport);
    probe->writes++;
    return probe->write_result;
}

static int unexpected_read(esp_transport_handle_t transport, char *buffer,
                            int length, int timeout_ms)
{
    (void)buffer;
    (void)length;
    (void)timeout_ms;
    transport_probe_t *probe = esp_transport_get_context_data(transport);
    probe->reads++;
    return -1;
}

TEST_CASE("Noise transport rejects incomplete writes before continuing the protocol", "[sv2][qemu-integration]")
{
    esp_transport_handle_t transport = esp_transport_init();
    TEST_ASSERT_NOT_NULL(transport);
    TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_func(transport, NULL, unexpected_read,
        incomplete_write, NULL, NULL, NULL, NULL));
    const int results[] = {-1, 0, 1, 63};
    for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); ++i) {
        transport_probe_t probe = {.write_result = results[i]};
        TEST_ASSERT_EQUAL(ESP_OK, esp_transport_set_context_data(transport, &probe));
        sv2_noise_ctx_t *noise = sv2_noise_create();
        TEST_ASSERT_NOT_NULL(noise);
        TEST_ASSERT_EQUAL(-1, sv2_noise_handshake(noise, transport, NULL));
        TEST_ASSERT_EQUAL_UINT32(1, probe.writes);
        TEST_ASSERT_EQUAL_UINT32(0, probe.reads);
        sv2_noise_destroy(noise);
    }
    esp_transport_destroy(transport);
}
