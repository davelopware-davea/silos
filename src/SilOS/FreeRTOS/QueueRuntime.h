#pragma once

#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t StorageRequestQueue;
extern QueueHandle_t StorageCompletionQueue;
extern QueueHandle_t BindReadyQueue;
extern QueueHandle_t ShellRequestQueue;
extern QueueHandle_t ShellEventQueue;

void silos_create_runtime_queues();
void silos_lock_ulisp_workspace();
void silos_unlock_ulisp_workspace();
void silos_storage_task(void *);
void silos_shell_task(void *);
void silos_ui_render_task(void *);
