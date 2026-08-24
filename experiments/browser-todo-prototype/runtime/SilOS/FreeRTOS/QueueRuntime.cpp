#include "SilOS/FreeRTOS/QueueRuntime.h"

#include "SilOS/Runtime/State.h"

#include <cstdio>

#include "task.h"

QueueHandle_t StorageRequestQueue = nullptr;
QueueHandle_t StorageCompletionQueue = nullptr;
QueueHandle_t BindReadyQueue = nullptr;
QueueHandle_t ShellRequestQueue = nullptr;
QueueHandle_t ShellEventQueue = nullptr;

void silos_create_runtime_queues() {
  StorageRequestQueue = xQueueCreate(1, sizeof(StorageRequest));
  StorageCompletionQueue = xQueueCreate(1, sizeof(StorageCompletion));
  BindReadyQueue = xQueueCreate(1, sizeof(bool));
  ShellRequestQueue = xQueueCreate(4, sizeof(ShellRequest));
  ShellEventQueue = xQueueCreate(4, sizeof(ShellEvent));
  configASSERT(StorageRequestQueue != nullptr);
  configASSERT(StorageCompletionQueue != nullptr);
  configASSERT(BindReadyQueue != nullptr);
  configASSERT(ShellRequestQueue != nullptr);
  configASSERT(ShellEventQueue != nullptr);
}

void silos_storage_task(void *) {
  for (;;) {
    StorageRequest request{};
    configASSERT(xQueueReceive(StorageRequestQueue, &request, portMAX_DELAY) == pdPASS);
    StorageCompletion completion{};
    completion.kind = request.kind;
    std::snprintf(completion.store_name, sizeof(completion.store_name), "%s",
                  request.store_name);
    configASSERT(xQueueSend(StorageCompletionQueue, &completion, portMAX_DELAY) == pdPASS);
  }
}

void silos_shell_task(void *) {
  for (;;) {
    ShellRequest request{};
    configASSERT(xQueueReceive(ShellRequestQueue, &request, portMAX_DELAY) == pdPASS);
    ShellEvent event{};
    event.kind = ShellEventKind::Poke;
    event.app_generation = request.app_generation;
    event.payload = request.payload;
    configASSERT(xQueueSend(ShellEventQueue, &event, portMAX_DELAY) == pdPASS);
  }
}
