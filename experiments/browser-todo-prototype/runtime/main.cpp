#include "arduino_compat.hpp"

#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include <emscripten.h>

#include "InMemoryStoreBackend.h"
#include "StoreInitLoader.h"
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
constexpr std::size_t StoreNameCapacity = InMemoryStoreNameCapacity;
constexpr configSTACK_DEPTH_TYPE ULispTaskStackBytes = 65536U;
constexpr configSTACK_DEPTH_TYPE ClientTaskStackBytes = 32768U;
constexpr std::size_t PokeStringCapacity = 49U;
constexpr std::size_t PokeSymbolCapacity = 17U;
constexpr std::size_t PokeListCapacity = 8U;
constexpr std::size_t PokeValueCapacity = 16U;
constexpr std::size_t PokeDepthCapacity = 4U;

// ShellEvent is deliberately a native value arena, never a uLisp object graph.
// Its fixed bounds are the lifecycle API's boundary contract, rather than an
// incidental property of the Browser implementation. Nodes refer only to
// bounded indices in this flat arena, so queue storage is finite and does not
// rely on recursive C++ object layout.
enum class SerializedValueKind { Nil, Boolean, Integer, Symbol, String, List };
constexpr std::uint8_t InvalidSerializedValueIndex = PokeValueCapacity;
struct SerializedValue {
  SerializedValueKind kind = SerializedValueKind::Nil;
  int integer = 0;
  char text[PokeStringCapacity]{};
  std::uint8_t children[PokeListCapacity]{};
  std::uint8_t child_count = 0;
};
struct SerializedPayload {
  SerializedValue values[PokeValueCapacity]{};
  std::uint8_t value_count = 0;
  std::uint8_t root = InvalidSerializedValueIndex;
};
enum class ShellEventKind { AppInitialise, Poke };
struct ShellEvent {
  ShellEventKind kind = ShellEventKind::AppInitialise;
  std::uint32_t app_generation = 0;
  SerializedPayload payload{};
};
struct ShellRequest {
  std::uint32_t app_generation = 0;
  SerializedPayload payload{};
};

struct AppDeclaration {
  char name[32];
  int ideal_width = 0;
  int ideal_height = 0;
  char entry[StoreNameCapacity];
  bool present = false;
};

enum class StorageRequestKind { BindStore };
struct StorageRequest {
  StorageRequestKind kind;
  char store_name[StoreNameCapacity];
};
struct StorageCompletion {
  StorageRequestKind kind;
  char store_name[StoreNameCapacity];
};

InMemoryStoreBackend SourceStores;
AppDeclaration CurrentDeclaration;
bool AppStarted = false;
QueueHandle_t StorageRequestQueue;
QueueHandle_t StorageCompletionQueue;
QueueHandle_t BindReadyQueue;
QueueHandle_t ShellRequestQueue;
QueueHandle_t ShellEventQueue;
std::uint32_t ActiveAppGeneration = 0;
bool AppInitialiseDelivered = false;
int AppEventCount = 0;
int AppPokeCount = 0;
bool AppObservedNoBindBeforeInit = false;
bool AppObservedStageOne = false;
bool AppObservedStageTwo = false;
bool AppObservedPokeFifo = false;
bool UiPendingRendered = false;
bool UiReadyRendered = false;
const InMemoryStore *CurrentSource = nullptr;
std::size_t CurrentRow = 0;
std::size_t CurrentCharacter = 0;
bool SourceAtEnd = false;
bool StoreBindStartedPending = false;
bool StoreBindCompletedReady = false;
bool StoreRefWatchRegistered = false;
bool StoreRefWatchOldSnapshotCreated = false;
int StoreRefWatchInvocationCount = 0;
int StoreRefWatchObservationCount = 0;
bool StoreRefWatchObservedReady = false;
bool StoreRefWatchObservedCount = false;
bool StoreRefWatchObservedOldPending = false;
bool StoreRefWatchObservedOldValueNil = false;
char StoreRefWatchObservedDescription[InMemoryFieldValueCapacity]{};

bool has_prefix(const char *text, const char *prefix) {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

// The temporary discovery rule accepts exactly apps/<name>/app.lisp.  It does
// not inspect arbitrary stores, and it has no meaning beyond this bootstrap.
bool is_app_manifest_name(const char *name) {
  constexpr char Prefix[] = "apps/";
  constexpr char Suffix[] = "/app.lisp";
  if (!has_prefix(name, Prefix)) return false;
  const char *app_name = name + std::strlen(Prefix);
  const char *slash = std::strchr(app_name, '/');
  return slash != app_name && slash != nullptr && std::strcmp(slash, Suffix) == 0;
}

}

// These functions are called by the small generated uLisp adapter.  The
// adapter is generated at build time; third-party/ulisp-esp remains unedited.
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

namespace {
int read_source_character() {
  while (CurrentSource != nullptr && CurrentRow < CurrentSource->row_count) {
    const InMemoryStoreField *text =
        SourceStores.find_field(CurrentSource->rows[CurrentRow], "text");
    // The source loader asks for a named field just like any other store
    // consumer. A malformed source row is an assertion in this bounded
    // startup import, not a second source-specific representation in backend.
    configASSERT(text != nullptr);
    const char *row = text->value;
    const char character = row[CurrentCharacter++];
    if (character != '\0') return static_cast<unsigned char>(character);
    ++CurrentRow;
    CurrentCharacter = 0;
    // A generic source row does not retain its input-file newline. Re-emit one
    // boundary here so ordinary ';' comments end before the next stored row.
    return '\n';
  }
  SourceAtEnd = true;
  return -1;
}

}

void silos_browser_surface_begin(const char *state, const char *message);
void silos_browser_surface_add_text(int row_index, const char *field_name,
                                    const char *value);

#include "ulisp-generated.inc"

// The Browser surface is a deliberately thin projection of the bounded UI
// model. It receives only the template renderer's already-resolved fields; it
// neither reads StoreRefs nor sends input or mutation requests back to Lisp.
void silos_browser_surface_begin(const char *state, const char *message) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const root = document.getElementById('silos-app');
    if (root == null) return;
    root.replaceChildren();
    const title = document.createElement('h1');
    title.textContent = 'SilOS to-dos';
    const summary = document.createElement('p');
    summary.className = 'silos-ui-state';
    summary.dataset.state = UTF8ToString($0);
    summary.textContent = UTF8ToString($1);
    root.append(title, summary);
    if (summary.dataset.state === 'ready') {
      const list = document.createElement('ul');
      list.id = 'silos-todo-list';
      list.setAttribute('aria-label', 'Bound to-do items');
      root.append(list);
    }
  }, state, message);
}

void silos_browser_surface_add_text(int row_index, const char *field_name,
                                    const char *value) {
  EM_ASM({
    if (typeof document === 'undefined') return;
    const list = document.getElementById('silos-todo-list');
    if (list == null) return;
    const index = String($0);
    let row = list.querySelector('li[data-row-index="' + index + '"]');
    if (row == null) {
      row = document.createElement('li');
      row.dataset.rowIndex = index;
      list.append(row);
    }
    const text = document.createElement('span');
    if ($1 === 0) {
      text.className = 'silos-template-literal';
      text.dataset.templateKind = 'literal';
    } else {
      text.className = 'silos-template-field';
      text.dataset.field = UTF8ToString($1);
    }
    text.textContent = UTF8ToString($2);
    row.append(text);
  }, row_index, field_name, value);
}

namespace {
void complete_store_bind(const StorageCompletion &completion) {
  const InMemoryStore *store = SourceStores.get(completion.store_name);
  configASSERT(store != nullptr);
  configASSERT(SilosBoundStoreRef != nil);
  configASSERT(std::strcmp(completion.store_name, SilosBoundStoreName) == 0);

  // The storage task never mutates Lisp objects. It sends this bounded
  // completion, and the uLisp task alone installs the live row references.
  object *value = silos_find_field(SilosBoundStoreRef, "value");
  object *metadata = silos_find_field(SilosBoundStoreRef, "meta");
  configASSERT(value != nil);
  configASSERT(metadata != nil);
  object *status = silos_find_field(cdr(metadata), "status");
  configASSERT(status != nil);

  // This is a separate, non-live record allocated while the bound ref is
  // still pending. It remains protected while constructing the ready rows and
  // while its watch runs, so the callback can compare old and live state.
  object *old_snapshot = silos_snapshot_store_ref(SilosBoundStoreRef);
  protect(old_snapshot);
  StoreRefWatchOldSnapshotCreated = true;
  object *old_value = silos_find_field(old_snapshot, "value");
  configASSERT(old_value != nil);
  StoreRefWatchObservedOldValueNil = cdr(old_value) == nil;

  cdr(value) = silos_make_store_row_refs(*store, SilosBoundFieldNames,
                                          SilosBoundFieldCount, SilosBoundStart,
                                          SilosBoundCount);
  cdr(status) = silos_symbol("ready");
  configASSERT(SilosBoundStoreWatch != NULL);
  object *watch_arguments = cons(SilosBoundStoreRef, cons(old_snapshot, nil));
  (void)apply(SilosBoundStoreWatch, watch_arguments, nullptr);
  ++StoreRefWatchInvocationCount;
  unprotect();
  StoreBindCompletedReady = true;
  // The list's internally owned StoreRef dependency is a dirty notification,
  // not the app's storage callback. Render only after the app watch returns.
  silos_render_ui();
  std::printf("store-bind=%s status=ready watch=%d\n", completion.store_name,
              StoreRefWatchInvocationCount);
  const bool ready = true;
  configASSERT(xQueueSend(BindReadyQueue, &ready, portMAX_DELAY) == pdPASS);
}

void process_storage_completions() {
  StorageCompletion completion{};
  while (xQueueReceive(StorageCompletionQueue, &completion, 0) == pdPASS) {
    if (completion.kind == StorageRequestKind::BindStore) {
      complete_store_bind(completion);
    }
  }
}

void process_one_shell_event() {
  ShellEvent event{};
  if (xQueueReceive(ShellEventQueue, &event, 0) == pdPASS) {
    silos_dispatch_shell_event(event);
  }
}

void storage_task(void *) {
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

void shell_task(void *) {
  for (;;) {
    ShellRequest request{};
    configASSERT(xQueueReceive(ShellRequestQueue, &request, portMAX_DELAY) == pdPASS);
    ShellEvent event{};
    event.kind = ShellEventKind::Poke;
    event.app_generation = request.app_generation;
    event.payload = request.payload;
    // The separate Shell task means a poke never re-enters the handler that
    // requested it. Queue order is preserved byte-for-byte.
    configASSERT(xQueueSend(ShellEventQueue, &event, portMAX_DELAY) == pdPASS);
  }
}

bool evaluate_source_store(const InMemoryStore &store) {
  CurrentSource = &store;
  CurrentRow = 0;
  CurrentCharacter = 0;
  SourceAtEnd = false;

  if (setjmp(toplevel_handler)) {
    ulisperror();
    return false;
  }

  for (;;) {
    object *form = readmain(read_source_character);
    if (SourceAtEnd && form == nil) return true;
    protect(form);
    (void)eval(form, nullptr);
    unprotect();
  }
}

bool bootstrap_apps() {
  bool found_manifest = false;
  bool loaded = true;
  SourceStores.visit([&](const InMemoryStore &store) {
    if (!is_app_manifest_name(store.name)) return;
    found_manifest = true;
    silos_cleanup_active_app();
    ++ActiveAppGeneration;
    CurrentDeclaration = AppDeclaration{};
    AppStarted = false;
    loaded = evaluate_source_store(store) && CurrentDeclaration.present && loaded;
    const InMemoryStore *entry = SourceStores.get(CurrentDeclaration.entry);
    loaded = entry != nullptr && evaluate_source_store(*entry) && AppStarted && loaded;
    std::printf("manifest=%s app=%s entry=%s started=%s\n", store.name,
                CurrentDeclaration.name, CurrentDeclaration.entry,
                AppStarted ? "yes" : "no");
  });
  return found_manifest && loaded;
}

void ulisp_task(void *) {
  setup();
  const bool booted = bootstrap_apps();
  configASSERT(booted);
  for (;;) {
    process_storage_completions();
    process_one_shell_event();
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
  configASSERT(xTaskCreate(ulisp_task, "ulisp", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(storage_task, "store", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(shell_task, "shell", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(client_task, "client", ClientTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  vTaskStartScheduler();
  return 0;
}
