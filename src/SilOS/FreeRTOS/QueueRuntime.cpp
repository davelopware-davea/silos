#include "SilOS/FreeRTOS/QueueRuntime.h"

#include "SilOS/Runtime/State.h"
#include "SilOS/UI/IPlatformRenderEngine.h"
#include "SilOS/UI/Renderer.h"

#include <cstdio>

#include "task.h"
#include "semphr.h"

QueueHandle_t StorageRequestQueue = nullptr;
QueueHandle_t StorageCompletionQueue = nullptr;
QueueHandle_t BindReadyQueue = nullptr;
QueueHandle_t ShellRequestQueue = nullptr;
QueueHandle_t ShellEventQueue = nullptr;
namespace {
SemaphoreHandle_t ULispWorkspaceMutex = nullptr;
}

void silos_create_runtime_queues() {
  StorageRequestQueue = xQueueCreate(1, sizeof(StorageRequest));
  StorageCompletionQueue = xQueueCreate(1, sizeof(StorageCompletion));
  BindReadyQueue = xQueueCreate(1, sizeof(bool));
  ShellRequestQueue = xQueueCreate(4, sizeof(ShellRequest));
  ShellEventQueue = xQueueCreate(4, sizeof(ShellEvent));
  ULispWorkspaceMutex = xSemaphoreCreateMutex();
  configASSERT(StorageRequestQueue != nullptr);
  configASSERT(StorageCompletionQueue != nullptr);
  configASSERT(BindReadyQueue != nullptr);
  configASSERT(ShellRequestQueue != nullptr);
  configASSERT(ShellEventQueue != nullptr);
  configASSERT(ULispWorkspaceMutex != nullptr);
}

void silos_lock_ulisp_workspace() {
  configASSERT(xSemaphoreTake(ULispWorkspaceMutex, portMAX_DELAY) == pdTRUE);
}

void silos_unlock_ulisp_workspace() {
  configASSERT(xSemaphoreGive(ULispWorkspaceMutex) == pdTRUE);
}

void silos_storage_task(void *) {
  for (;;) {
    StorageRequest request{};
    configASSERT(xQueueReceive(StorageRequestQueue, &request, portMAX_DELAY) == pdPASS);
    StorageCompletion completion{};
    completion.kind = request.kind;
    completion.app_index = request.app_index;
    completion.app_generation = request.app_generation;
    configASSERT(xQueueSend(StorageCompletionQueue, &completion, portMAX_DELAY) == pdPASS);
  }
}

void silos_shell_task(void *) {
  for (;;) {
    ShellRequest request{};
    configASSERT(xQueueReceive(ShellRequestQueue, &request, portMAX_DELAY) == pdPASS);
    ShellEvent event{};
    event.kind = ShellEventKind::Poke;
    event.app_index = request.app_index;
    event.app_generation = request.app_generation;
    event.payload = request.payload;
    configASSERT(xQueueSend(ShellEventQueue, &event, portMAX_DELAY) == pdPASS);
  }
}

void silos_ui_render_task(void *) {
  const TickType_t period = pdMS_TO_TICKS(
      silos_platform_render_engine().framePeriodMilliseconds());
  for (;;) {
    silos_render_ui();
    vTaskDelay(period == 0 ? 1 : period);
  }
}
