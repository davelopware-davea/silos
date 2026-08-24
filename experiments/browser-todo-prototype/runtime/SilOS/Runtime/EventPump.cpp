#include "SilOS/Runtime/EventPump.h"

#include "SilOS/FreeRTOS/QueueRuntime.h"
#include "SilOS/Runtime/State.h"
#include "SilOS/uLisp/RuntimeAdapter.h"

void silos_process_runtime_events() {
  StorageCompletion completion{};
  while (xQueueReceive(StorageCompletionQueue, &completion, 0) == pdPASS) {
    if (completion.kind == StorageRequestKind::BindStore) {
      silos_ulisp_complete_store_bind(completion);
    }
  }
  ShellEvent event{};
  if (xQueueReceive(ShellEventQueue, &event, 0) == pdPASS) {
    silos_ulisp_dispatch_shell_event(event);
  }
}
