#include "arduino_compat.hpp"

#include <climits>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <new>

#include <emscripten.h>

#include "SilOS/Store/InMemoryStoreBackend.h"
#include "SilOS/Platform/Browser/BrowserStoreInitLoader.h"
#include "SilOS/UI/PlatformSurface.h"
#include "SilOS/UI/Renderer.h"
#include "SilOS/Runtime/State.h"
#include "SilOS/Runtime/AppBootstrap.h"
#include "SilOS/Runtime/EventPump.h"
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
}

// These functions are called by the SilOS uLisp extension included through
// the explicit integration seam in the vendored sketch.
void silos_capture_app_declaration(const char *name, int ideal_width,
                                   int ideal_height, const char *entry) {
  std::snprintf(CurrentDeclaration.name, sizeof(CurrentDeclaration.name), "%s", name);
  CurrentDeclaration.ideal_width = ideal_width;
  CurrentDeclaration.ideal_height = ideal_height;
  std::snprintf(CurrentDeclaration.entry, sizeof(CurrentDeclaration.entry), "%s", entry);
  CurrentDeclaration.present = true;
}

void silos_record_app_start() { AppStarted = true; }

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
  setup();
  const bool booted = silos_bootstrap_apps(SourceStores);
  configASSERT(booted);
  for (;;) {
    silos_process_runtime_events();
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
  const bool observed_description = std::strcmp(
      StoreRefWatchObservedDescription,
      "Learn how SilOS loads Lisp from a store") == 0;
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
                      AppEventCount == 3 && AppPokeCount == 2 &&
                      SilosUiTypeDeclared && SilosUiRefDeclared &&
                      SilosUiItemTemplateDeclared && SilosUiLiteralDeclared &&
                      SilosUiListDeclared && SilosUiMounted && UiReadyRendered &&
                      SilosUiLiteralRendered;
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
  // Emscripten preloads runtime/store-init at /store-init before MAIN runs.
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
  configASSERT(xTaskCreate(client_task, "client", ClientTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  vTaskStartScheduler();
  return 0;
}
