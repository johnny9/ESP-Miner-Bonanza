#ifndef ESP_MINER_HOST_FREERTOS_TASK_H
#define ESP_MINER_HOST_FREERTOS_TASK_H

#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

/*
 * Declaration only: tests that execute task timing must provide a behavioral
 * scheduler adapter instead of silently using a no-op delay.
 */
void vTaskDelay(TickType_t ticks);

#endif
