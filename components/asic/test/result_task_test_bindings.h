#ifndef RESULT_TASK_TEST_BINDINGS_H_
#define RESULT_TASK_TEST_BINDINGS_H_

#include "../../stratum/test/job_test_state.h"

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef vTaskDelay
#undef vTaskDelay
#endif
#define vTaskDelay result_task_spy_delay
#define ASIC_result_task result_task_test_run
#define ASIC_process_work result_task_fake_process_work
#define ASIC_record_local_result result_task_spy_local_result
#define stratum_work_is_current result_task_fake_work_is_current
#define stratum_queue_share result_task_fake_queue_share
#define self_test_record_nonce result_task_spy_record_nonce
#define SYSTEM_notify_found_nonce result_task_spy_notify_found_nonce
#define scoreboard_add result_task_spy_scoreboard_add
#define hashrate_monitor_register_read result_task_spy_register_read

void result_task_spy_delay(TickType_t ticks);

void result_task_spy_register_read(void *state, register_type_t type,
                                   uint8_t asic_nr, uint32_t value, uint64_t timestamp);
void result_task_spy_notify_found_nonce(GlobalState *state, double difficulty, uint32_t target);
esp_err_t result_task_spy_scoreboard_add(Scoreboard *scoreboard, double difficulty,
                                        const char *job_id, const char *extranonce,
                                        uint32_t ntime, uint32_t nonce, uint32_t version_bits);
#include "../../../main/tasks/asic_result_task.h"

#endif /* RESULT_TASK_TEST_BINDINGS_H_ */
