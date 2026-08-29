#include "arduino_compat.hpp"

#include <climits>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>
#include <string>
#include <utility>

#include <emscripten.h>

#include "SilOS/Store/InMemoryStoreBackend.h"
#include "SilOS/Platform/Browser/BrowserStoreInitLoader.h"
#include "SilOS/UI/IPlatformRenderEngine.h"
#include "SilOS/UI/Renderer.h"
#include "SilOS/UI/UIAppBinding.h"
#include "SilOS/Runtime/State.h"
#include "SilOS/Runtime/AppBootstrap.h"
#include "SilOS/Runtime/EventPump.h"
#include "SilOS/uLisp/RuntimeAdapter.h"
#include "SilOS/Shell/Events.h"
#include "SilOS/FreeRTOS/QueueRuntime.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

HardwareSerial Serial;
HardwareSerial Serial1;
SPIClass SPI;
TwoWire Wire;
TwoWire Wire1;
LittleFSClass LittleFS;
WiFiClass WiFi;

namespace {
constexpr configSTACK_DEPTH_TYPE ULispTaskStackBytes = 65536U;
constexpr configSTACK_DEPTH_TYPE ClientTaskStackBytes = 32768U;
constexpr configSTACK_DEPTH_TYPE RenderTaskStackBytes = 32768U;
}

// These functions are called by the SilOS uLisp extension included through
// the explicit integration seam in the vendored sketch.
void silos_capture_app_declaration(sobject *display_name, std::string name,
                                   int ideal_width, int ideal_height,
                                   std::string entry) {
  configASSERT(CurrentAppIndex < AppCount);
  AppDeclaration &declaration = AppDeclarations[CurrentAppIndex];
  declaration.name = std::move(name);
  declaration.display_name = display_name;
  declaration.ideal_width = ideal_width;
  declaration.ideal_height = ideal_height;
  declaration.entry = std::move(entry);
  declaration.present = true;
}

void silos_record_app_on_event() {
  configASSERT(CurrentAppIndex < AppCount);
  AppStarted[CurrentAppIndex] = true;
}

void silos_before_load_image() { silos_cleanup_apps(); }
bool silos_after_load_image() { return silos_bootstrap_apps(SourceStores); }

void silos_serial_write(char value) {
  std::putchar(static_cast<unsigned char>(value));
}

// The copied Browser compatibility header still uses FreeWisp's original
// callback name. Keep that adapter detail local to this runtime.
void freewisp_serial_write(char value) { silos_serial_write(value); }

unsigned long millis() { return static_cast<unsigned long>(emscripten_get_now()); }
unsigned long micros() { return static_cast<unsigned long>(emscripten_get_now() * 1000.0); }
void delay(unsigned long milliseconds) {
  TickType_t ticks = pdMS_TO_TICKS(milliseconds);
  vTaskDelay(ticks == 0 ? 1 : ticks);
}
void randomSeed(unsigned long seed) { std::srand(seed); }
long random(long upper) { return upper > 0 ? std::rand() % upper : 0; }
long random(long lower, long upper) { return lower + random(upper - lower); }
void pinMode(int, int) {}
int digitalRead(int) { return LOW; }
void digitalWrite(int, int) {}
int analogRead(int) { return 0; }
void analogWrite(int, int) {}
void analogReadResolution(int) {}
void analogWriteResolution(int) {}
void dacWrite(int, int) {}
void yield() { taskYIELD(); }
void esp_light_sleep_start() {}
void esp_sleep_enable_timer_wakeup(std::uint64_t) {}
void configTime(long, int, const char *) {}

#include "ulisp-generated.inc"

namespace {
void ulisp_task(void *) {
  silos_lock_ulisp_workspace();
  setup();
  const bool booted = silos_bootstrap_apps(SourceStores);
  silos_unlock_ulisp_workspace();
  configASSERT(booted);
  for (;;) {
    silos_lock_ulisp_workspace();
    silos_process_runtime_events();
    silos_unlock_ulisp_workspace();
    // One whole tick gives the cooperative Browser scheduler a yield point.
    // pdMS_TO_TICKS(1) rounds down to zero at this target's 100 Hz tick rate.
    vTaskDelay(1);
  }
}

void client_task(void *) {
  // Wait for the uLisp task's completion notification, rather than assuming a
  // duration. This makes the pending-to-ready proof deterministic.
  bool bind_ready = false;
  configASSERT(xQueueReceive(BindReadyQueue, &bind_ready, portMAX_DELAY) == pdPASS);
  // The readiness notification is intentionally sent after the StoreRef watch
  // but before the next Shell turn. Give that independently queued stage a
  // bounded chance to run before this test-only client decides the proof.
  for (int ticks = 0; ticks < 20 && !AppObservedStageTwo; ++ticks) vTaskDelay(1);
  // Rendering has its own cadence and intentionally samples app state rather
  // than rendering synchronously from a declaration or Store completion.
  for (int ticks = 0;
       ticks < 20 && (!UiReadyRendered || SilosRenderedAppCount != AppCount);
       ++ticks) {
    vTaskDelay(1);
  }
  const bool observed_description = std::strcmp(
      StoreRefWatchObservedDescription.c_str(),
      "Learn how SilOS loads Lisp from a store") == 0;
  std::size_t todo_app = SilosInvalidAppIndex;
  std::size_t status_app = SilosInvalidAppIndex;
  std::size_t mounted_template_count = 0;
  for (std::size_t index = 0; index < AppCount; ++index) {
    if (AppDeclarations[index].name == "To-do") todo_app = index;
    if (AppDeclarations[index].name == "Status") status_app = index;
    mounted_template_count += silos_ui_binding(index).mountCount();
  }
  const bool passed = bind_ready && StoreBindStartedPending &&
                      StoreBindCompletedReady && StoreRefWatchRegistered &&
                      StoreRefWatchOldSnapshotCreated &&
                      StoreRefWatchInvocationCount == 1 &&
                      StoreRefWatchObservationCount == 1 &&
                      StoreRefWatchObservedReady && StoreRefWatchObservedCount &&
                      observed_description && StoreRefWatchObservedOldPending &&
                      StoreRefWatchObservedOldValueNil && AppInitialiseDelivered &&
                      AppObservedNoBindBeforeInit && AppObservedStageOne &&
                      AppObservedStageTwo && AppObservedPokeFifo &&
                      AppEventCount >= 4 && AppPokeCount == 2 &&
                      AppCount >= 2 && todo_app < AppCount && status_app < AppCount &&
                      silos_ui_binding(todo_app).typeCount() == 1 &&
                      silos_ui_binding(todo_app).refCount() == 1 &&
                      silos_ui_binding(todo_app).templateCount() == 3 &&
                      silos_ui_binding(todo_app).mountCount() == 2 &&
                      silos_ui_binding(status_app).typeCount() == 0 &&
                      silos_ui_binding(status_app).refCount() == 0 &&
                      silos_ui_binding(status_app).templateCount() == 1 &&
                      silos_ui_binding(status_app).mountCount() == 1 &&
                      SilosRenderedAppCount == AppCount &&
                      SilosRenderedMountCount == mounted_template_count &&
                      SilosRenderedListRowCount >= 2 &&
                      SilosRenderedInstructionCount >= 9 && UiReadyRendered;
  std::printf("store-watch fired=%d ready=%s count=%s old=pending/%s\n",
              StoreRefWatchInvocationCount,
              StoreRefWatchObservedReady ? "yes" : "no",
              StoreRefWatchObservedCount ? "yes" : "no",
              StoreRefWatchObservedOldValueNil ? "nil" : "not-nil");
  std::printf("app-events initialise=%s pokes=%d fifo=%s no-bind-before-init=%s stages=%s/%s\n",
              AppInitialiseDelivered ? "yes" : "no", AppPokeCount,
              AppObservedPokeFifo ? "yes" : "no",
              AppObservedNoBindBeforeInit ? "yes" : "no",
              AppObservedStageOne ? "one" : "missing",
              AppObservedStageTwo ? "two" : "missing");
  std::puts(passed ? "SILOS_TODO_BOOT_PASS" : "SILOS_TODO_BOOT_FAIL");
  if (!passed) std::abort();
  vTaskEndScheduler();
  std::abort();
}
}

extern "C" void vSilOSAssert(const char *file, int line) {
  std::fprintf(stderr, "SilOS assertion failed at %s:%d\n", file, line);
  std::abort();
}

extern "C" void vApplicationMallocFailedHook() { vSilOSAssert("FreeRTOS heap exhausted", 0); }
extern "C" void vApplicationIdleHook() { vPortWaitForTick(); }

int main() {
  StoreInitLoadResult store_init{};
  // Emscripten preloads store-init at /store-init before MAIN runs.
  // The importer traverses that virtual directory, so the startup proof reads
  // the versioned files rather than any compiled C++ source constants.
  configASSERT(load_store_init("/store-init", SourceStores, store_init));
  std::printf("store-init stores=%zu source-rows=%zu csv-rows=%zu\n",
              store_init.store_count, store_init.source_row_count,
              store_init.csv_row_count);
  silos_create_runtime_queues();
  configASSERT(xTaskCreate(ulisp_task, "ulisp", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(silos_storage_task, "store", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(silos_shell_task, "shell", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(silos_ui_render_task, "ui-render", RenderTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(client_task, "client", ClientTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  vTaskStartScheduler();
  return 0;
}
