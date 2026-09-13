#include "bm1397_test_bindings.h"
#include "bm1397_test_harness.h"

#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include "bm1397.h"
#include "frequency_transition_bmXX.h"
#include "global_state.h"
#include "mining.h"
#include "serial.h"
#include "unity.h"

const bm1397_harness_driver_t bm1397_harness_driver = {
    BM1397_init, BM1397_send_work, BM1397_process_work,
};

enum { MAX_PACKETS = 32, JOB_SLOTS = 128 };
static GlobalState fixture_state;
static bm1397_harness_packet_t packets[MAX_PACKETS];
static size_t packet_count;
static uint8_t queued_response[BM1397_HARNESS_RESPONSE_SIZE];
static bool response_ready;

GlobalState *bm1397_harness_begin(void)
{
    fixture_state = (GlobalState) {
        .DEVICE_CONFIG.family.asic_count = 2,
        .DEVICE_CONFIG.family.asic.difficulty = 256,
        .DEVICE_CONFIG.family.asic.core_count = 672,
        .POWER_MANAGEMENT_MODULE.frequency_value = 200.0f,
    };
    TEST_ASSERT_TRUE(asic_job_store_init(&fixture_state.asic_job_store));
    packet_count = 0;
    response_ready = false;
    return &fixture_state;
}

void bm1397_harness_end(void)
{
    /* Detect a driver returning while it still owns the shared store lock. */
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_trylock(&fixture_state.asic_job_store.lock));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_unlock(&fixture_state.asic_job_store.lock));
    asic_job_store_destroy(&fixture_state.asic_job_store);
}

void bm1397_harness_clear_packets(void)
{
    packet_count = 0;
}

size_t bm1397_harness_packet_count(void)
{
    return packet_count;
}

const bm1397_harness_packet_t *bm1397_harness_packet(size_t index)
{
    TEST_ASSERT_TRUE(index < packet_count);
    return &packets[index];
}

bool bm1397_harness_snapshot(uint8_t job_id, asic_job_t *work)
{
    return asic_job_store_snapshot(&fixture_state.asic_job_store, job_id, work);
}

void bm1397_harness_install_job(uint8_t job_id, const asic_job_t *job)
{
    TEST_ASSERT_TRUE(asic_job_store_store_slot(&fixture_state.asic_job_store, job_id, job, NULL));
}

void bm1397_harness_queue_response(
    const uint8_t response[BM1397_HARNESS_RESPONSE_SIZE])
{
    response_ready = response != NULL;
    if (response_ready) {
        memcpy(queued_response, response, sizeof(queued_response));
    }
}

int bm1397_spy_serial_send(uint8_t *bytes, int length, bool debug)
{
    (void)debug;
    TEST_ASSERT_TRUE(packet_count < MAX_PACKETS);
    TEST_ASSERT_TRUE(length > 0 && (size_t)length <= sizeof(packets[0].bytes));
    bm1397_harness_packet_t *packet = &packets[packet_count++];
    packet->length = (size_t)length;
    memcpy(packet->bytes, bytes, packet->length);
    return length;
}

esp_err_t bm1397_stub_serial_set_baud(int baud)
{
    (void)baud;
    return ESP_OK;
}

void bm1397_stub_serial_clear_buffer(void)
{
}

esp_err_t bm1397_fake_receive_work(uint8_t *buffer, int size,
                                  uint64_t *timestamp_us)
{
    TEST_ASSERT_EQUAL_INT(sizeof(queued_response), size);
    if (!response_ready) {
        return ESP_FAIL;
    }
    memcpy(buffer, queued_response, sizeof(queued_response));
    response_ready = false;
    *timestamp_us = UINT64_C(123456789);
    return ESP_OK;
}

int bm1397_stub_count_chips(uint16_t count, uint16_t chip_id,
                           int response_length)
{
    TEST_ASSERT_EQUAL_HEX16(0x1397, chip_id);
    TEST_ASSERT_EQUAL_INT(BM1397_HARNESS_RESPONSE_SIZE, response_length);
    return count;
}

void bm1397_stub_frequency_transition(
    GlobalState *state, set_hash_frequency_fn set_frequency)
{
    (void)state;
    (void)set_frequency;
}

void bm1397_stub_delay(TickType_t ticks)
{
    (void)ticks;
}

void bm1397_harness_mark_job_valid(uint8_t job_id)
{
    asic_job_store_entry_t *entry = &fixture_state.asic_job_store.entries[job_id];
    entry->valid = true;
    entry->handle = ASIC_WORK_HANDLE_INVALID;
}
