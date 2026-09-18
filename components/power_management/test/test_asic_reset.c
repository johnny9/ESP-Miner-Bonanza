#include "asic_reset_backend.h"
#include "unity.h"

#include <string.h>

typedef struct {
    unsigned holds, pulses;
    uint32_t low_ms, release_ms;
    esp_err_t result;
} reset_fixture_t;

static reset_fixture_t gpio;

static esp_err_t hold(void *context)
{
    reset_fixture_t *fixture = context;
    ++fixture->holds;
    return fixture->result;
}

static esp_err_t pulse(void *context, uint32_t low_ms, uint32_t release_ms)
{
    reset_fixture_t *fixture = context;
    ++fixture->pulses;
    fixture->low_ms = low_ms;
    fixture->release_ms = release_ms;
    return fixture->result;
}

/* Link-time platform port: the production selector and legacy entry points
 * are exercised without driving GPIO or a physical bridge. */
const asic_reset_backend_t ASIC_RESET_GPIO_BACKEND = {
    .context = &gpio, .hold_low = hold, .pulse = pulse,
};

TEST_CASE("Legacy reset entry points retain GPIO timing and error behavior", "[power-management][reset]")
{
    gpio = (reset_fixture_t){0};
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset_configure(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, asic_hold_reset_low());
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset(10, 100));
    TEST_ASSERT_EQUAL_UINT(1, gpio.holds);
    TEST_ASSERT_EQUAL_UINT(1, gpio.pulses);
    TEST_ASSERT_EQUAL_UINT32(10, gpio.low_ms);
    TEST_ASSERT_EQUAL_UINT32(100, gpio.release_ms);
    gpio.result = ESP_FAIL;
    TEST_ASSERT_EQUAL(ESP_FAIL, asic_hold_reset_low());
    TEST_ASSERT_EQUAL(ESP_FAIL, asic_reset(1, 2));
    gpio = (reset_fixture_t){0};
}

TEST_CASE("Configured reset failures never fall back to direct GPIO", "[power-management][reset]")
{
    gpio = (reset_fixture_t){0};
    reset_fixture_t bridge = {.result = ESP_FAIL};
    asic_reset_backend_t backend = {.context = &bridge, .hold_low = hold, .pulse = pulse};
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset_configure(&backend));
    memset(&backend, 0, sizeof(backend)); // Configuration is copied.
    TEST_ASSERT_EQUAL(ESP_FAIL, asic_hold_reset_low());
    TEST_ASSERT_EQUAL(ESP_FAIL, asic_reset(17, 23));
    TEST_ASSERT_EQUAL_UINT(1, bridge.holds);
    TEST_ASSERT_EQUAL_UINT(1, bridge.pulses);
    TEST_ASSERT_EQUAL_UINT32(17, bridge.low_ms);
    TEST_ASSERT_EQUAL_UINT32(23, bridge.release_ms);
    TEST_ASSERT_EQUAL_UINT(0, gpio.holds + gpio.pulses);
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset_configure(NULL));
}

TEST_CASE("Incomplete reset backends cannot replace the selected board", "[power-management][reset]")
{
    reset_fixture_t bridge = {0};
    asic_reset_backend_t backend = {.context = &bridge, .hold_low = hold, .pulse = pulse};
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset_configure(&backend));
    backend.pulse = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, asic_reset_configure(&backend));
    backend.pulse = pulse;
    backend.hold_low = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, asic_reset_configure(&backend));
    TEST_ASSERT_EQUAL(ESP_OK, asic_hold_reset_low());
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset(1, 2));
    TEST_ASSERT_EQUAL_UINT(1, bridge.holds);
    TEST_ASSERT_EQUAL_UINT(1, bridge.pulses);
    TEST_ASSERT_EQUAL(ESP_OK, asic_reset_configure(NULL));
    gpio = (reset_fixture_t){0};
    TEST_ASSERT_EQUAL(ESP_OK, asic_hold_reset_low());
    TEST_ASSERT_EQUAL_UINT(1, gpio.holds);
}
