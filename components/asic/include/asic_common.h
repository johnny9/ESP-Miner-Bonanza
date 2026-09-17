#ifndef ASIC_COMMON_H_
#define ASIC_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "asic_result.h"

typedef struct GlobalState GlobalState;
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ASIC_INIT_CORE_REGISTER_DELAY_MS 10

static inline void asic_init_core_register_delay(void)
{
    vTaskDelay(pdMS_TO_TICKS(ASIC_INIT_CORE_REGISTER_DELAY_MS));
}

static const double NONCE_SPACE = 4294967296.0; //  2^32

typedef struct
{
    asic_job_t job;
    asic_job_context_t context;
    // Hardware slot remains private to the Bitmain adapter.
    uint8_t job_id;
    uint32_t nonce;
    uint32_t ntime;
    uint32_t rolled_version;
    uint32_t version_bits;
    // Register response.
    register_type_t register_type;
    uint8_t asic_nr;
    uint32_t value;
    uint8_t core_id;
    uint8_t small_core_id;
    // Receive timestamp.
    uint64_t timestamp_us;
} task_result;

unsigned char _reverse_bits(unsigned char num);
int _largest_power_of_two(int num);
int _next_power_of_two(int num);
void clear_asic_chain_error(void);
const char *get_asic_chain_error(void);
int count_asic_chips(uint16_t asic_count, uint16_t chip_id, int chip_id_response_length);
int count_asic_chips_with_id_alias(uint16_t asic_count, uint16_t chip_id, uint16_t chip_id_alias, int chip_id_response_length);
esp_err_t receive_work(uint8_t * buffer, int buffer_size, uint64_t *out_timestamp_us);
void get_difficulty_mask(double difficulty, uint8_t *job_difficulty_mask);
double calculate_bm_timeout_ms(float frequency_mhz, size_t asic_count, size_t small_cores, size_t cores, size_t version_size, float timeout_percent, double default_time_ms);

#endif /* ASIC_COMMON_H_ */
