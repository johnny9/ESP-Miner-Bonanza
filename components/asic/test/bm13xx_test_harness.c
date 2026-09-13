#include "bm13xx_test_bindings.h"
#include "bm13xx_test_harness.h"

#include <pthread.h>
#include <string.h>
#include "bm1366.h"
#include "bm1368.h"
#include "bm1370.h"
#include "bm1373.h"
#include "frequency_transition_bmXX.h"
#include "global_state.h"
#include "mining.h"
#include "serial.h"
#include "unity.h"

const bm13xx_harness_driver_t
    bm13xx_harness_drivers[BM13XX_HARNESS_DRIVER_COUNT] = {
        {"BM1366", BM1366_init, BM1366_send_work, BM1366_set_version_mask, BM1366_process_work, 0x13, 4},
        {"BM1368", BM1368_init, BM1368_send_work, BM1368_set_version_mask, BM1368_process_work, 0x23, 5},
        {"BM1370", BM1370_init, BM1370_send_work, BM1370_set_version_mask, BM1370_process_work, 0x23, 4},
        {"BM1373", BM1373_init, BM1373_send_work, BM1373_set_version_mask, BM1373_process_work, 0x23, 1},
    };

enum { MAX_PACKETS = 128, JOB_SLOT = 0x10, JOB_SLOTS = 128 };
static GlobalState fixture_state;
static bm13xx_harness_packet_t packets[MAX_PACKETS];
static size_t packet_count;
static uint8_t queued_response[BM13XX_HARNESS_RESPONSE_SIZE];
static bool response_ready;
static unsigned failed_writes;
static unsigned delay_count;

GlobalState *bm13xx_harness_begin(void)
{
    fixture_state = (GlobalState) {
        .DEVICE_CONFIG.family.asic_count = 2,
        .DEVICE_CONFIG.family.voltage_domains = 1,
        .DEVICE_CONFIG.family.asic.difficulty = 256,
        .DEVICE_CONFIG.family.asic.core_count = 112,
        .POWER_MANAGEMENT_MODULE.frequency_value = 200.0f,
        .POWER_MANAGEMENT_MODULE.actual_frequency = 200.0f,
    };
    TEST_ASSERT_TRUE(asic_job_store_init(&fixture_state.asic_job_store));
    packet_count = 0;
    response_ready = false;
    failed_writes = 0;
    delay_count = 0;
    return &fixture_state;
}

void bm13xx_harness_end(void)
{
    /* Detect a driver returning while it still owns the shared store lock. */
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_trylock(&fixture_state.asic_job_store.lock));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_unlock(&fixture_state.asic_job_store.lock));
    asic_job_store_destroy(&fixture_state.asic_job_store);
}

void bm13xx_harness_clear_packets(void)
{
    packet_count = 0;
    delay_count = 0;
}

size_t bm13xx_harness_packet_count(void)
{
    return packet_count;
}

const bm13xx_harness_packet_t *bm13xx_harness_packet(size_t index)
{
    TEST_ASSERT_TRUE(index < packet_count);
    return &packets[index];
}

bool bm13xx_harness_snapshot(uint8_t job_id, mining_template_t *work)
{
    return asic_job_store_snapshot(&fixture_state.asic_job_store, job_id, work);
}

void bm13xx_harness_install_job(uint8_t job_id, const mining_template_t *job)
{
    TEST_ASSERT_TRUE(asic_job_store_store_slot(&fixture_state.asic_job_store, job_id, job, NULL));
}

void bm13xx_harness_fail_writes(unsigned count)
{
    failed_writes = count;
}

unsigned bm13xx_harness_delay_count(void)
{
    return delay_count;
}

void bm13xx_harness_set_job(uint32_t version, bool valid, bool present)
{
    asic_job_store_release(&fixture_state.asic_job_store, JOB_SLOT);
    if (valid && present) {
        mining_template_t job = {.version = version, .version_mask = 0x1fffe000,
                                 .ntime = 0x64658bd8};
        bm13xx_harness_install_job(JOB_SLOT, &job);
    }
}

void bm13xx_harness_queue_response(
    const uint8_t response[BM13XX_HARNESS_RESPONSE_SIZE])
{
    response_ready = response != NULL;
    if (response_ready) {
        memcpy(queued_response, response, sizeof(queued_response));
    }
}

int bm13xx_spy_serial_send(uint8_t *bytes, int length, bool debug)
{
    (void)debug;
    TEST_ASSERT_TRUE(packet_count < MAX_PACKETS);
    TEST_ASSERT_TRUE(length > 0 && (size_t)length <= sizeof(packets[0].bytes));
    bm13xx_harness_packet_t *packet = &packets[packet_count++];
    packet->length = (size_t)length;
    memcpy(packet->bytes, bytes, packet->length);
    if (failed_writes > 0) {
        failed_writes--;
        return length - 1;
    }
    return length;
}

esp_err_t bm13xx_stub_serial_set_baud(int baud)
{
    TEST_ASSERT_EQUAL_INT(3000000, baud);
    return ESP_OK;
}

void bm13xx_stub_serial_clear_buffer(void)
{
}

esp_err_t bm13xx_fake_receive_work(uint8_t *buffer, int size,
                                  uint64_t *timestamp_us)
{
    TEST_ASSERT_EQUAL_INT(sizeof(queued_response), size);
    if (!response_ready) {
        return ESP_FAIL;
    }
    memcpy(buffer, queued_response, sizeof(queued_response));
    response_ready = false;
    *timestamp_us = 123456789;
    return ESP_OK;
}

int bm13xx_stub_count_chips(uint16_t count, uint16_t chip_id,
                           int response_length)
{
    (void)chip_id;
    TEST_ASSERT_EQUAL_INT(11, response_length);
    return count;
}

int bm13xx_stub_count_chips_with_alias(
    uint16_t count, uint16_t chip_id, uint16_t alias, int response_length)
{
    TEST_ASSERT_EQUAL_HEX16(0x1372, chip_id);
    TEST_ASSERT_EQUAL_HEX16(0x1373, alias);
    TEST_ASSERT_EQUAL_INT(9, response_length);
    return count;
}

void bm13xx_stub_frequency_transition(
    GlobalState *state, set_hash_frequency_fn set_frequency)
{
    /* Clock ramps are outside the version-rolling contract. */
    (void)state;
    (void)set_frequency;
}

void bm13xx_spy_delay(TickType_t ticks)
{
    (void)ticks;
    delay_count++;
}
