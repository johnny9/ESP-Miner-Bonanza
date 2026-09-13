#ifndef ESP_MINER_HOST_FREERTOS_SEMPHR_H
#define ESP_MINER_HOST_FREERTOS_SEMPHR_H
/* Only the opaque handle is needed; no semaphore behavior is emulated. */
typedef struct host_semaphore *SemaphoreHandle_t;
#endif
