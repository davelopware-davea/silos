#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void vSilOSAssert(const char *file, int line);
#ifdef __cplusplus
}
#endif

#define configASSERT(condition) do { if (!(condition)) vSilOSAssert(__FILE__, __LINE__); } while (0)
#define configUSE_PREEMPTION 0
#define configUSE_TIME_SLICING 0
#define configUSE_IDLE_HOOK 1
#define configUSE_TICK_HOOK 0
#define configCPU_CLOCK_HZ 1000000UL
#define configTICK_RATE_HZ 100U
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configSTACK_DEPTH_TYPE uint32_t
#define configMAX_PRIORITIES 3
#define configMINIMAL_STACK_SIZE 16384U
#define configMAX_TASK_NAME_LEN 16
#define configTOTAL_HEAP_SIZE (512U * 1024U)
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_MUTEXES 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_TASK_NOTIFICATIONS 0
#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_TIMERS 0
#define configEMSCRIPTEN_MAX_TASKS 4U
#define configEMSCRIPTEN_ASYNCIFY_STACK_BYTES 16384U
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1

#endif
