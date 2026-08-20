#include "arduino_compat.hpp"

#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include <emscripten.h>

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
constexpr std::size_t StoreNameCapacity = 48;
constexpr std::size_t SourceRowCapacity = 40;
constexpr std::size_t TodoRowCapacity = 8;
constexpr std::size_t StoreCapacity = 8;
constexpr std::size_t ExpressionCapacity = 128;
constexpr std::size_t OutputCapacity = 256;
constexpr configSTACK_DEPTH_TYPE ULispTaskStackBytes = 65536U;
constexpr configSTACK_DEPTH_TYPE ClientTaskStackBytes = 32768U;

enum class StoreKind { Source, TodoItems };

struct TodoRow {
  int id = 0;
  const char *description = nullptr;
  const char *status = nullptr;
};

struct InMemoryStore {
  char name[StoreNameCapacity];
  StoreKind kind = StoreKind::Source;
  const char *source_rows[SourceRowCapacity]{};
  std::size_t source_row_count = 0;
  TodoRow todo_rows[TodoRowCapacity]{};
  std::size_t todo_row_count = 0;
};

// This deliberately small SRAM-style backend owns both source stores and the
// one volatile data store needed by this experiment.  Its capacities are fixed
// so the Browser proof continues to exercise the MCU-shaped bounded model.
class InMemoryStoreBackend {
public:
  bool create_source(const char *name) {
    if (count_ == std::size(stores_)) return false;
    InMemoryStore &store = stores_[count_++];
    std::snprintf(store.name, sizeof(store.name), "%s", name);
    store.kind = StoreKind::Source;
    return true;
  }

  bool append_source_row(const char *name, const char *text) {
    InMemoryStore *store = find(name);
    if (store == nullptr || store->kind != StoreKind::Source ||
        store->source_row_count == std::size(store->source_rows)) return false;
    store->source_rows[store->source_row_count++] = text;
    return true;
  }

  bool create_todo_items(const char *name) {
    if (count_ == std::size(stores_)) return false;
    InMemoryStore &store = stores_[count_++];
    std::snprintf(store.name, sizeof(store.name), "%s", name);
    store.kind = StoreKind::TodoItems;
    return true;
  }

  bool append_todo_row(const char *name, int id, const char *description,
                       const char *status) {
    InMemoryStore *store = find(name);
    if (store == nullptr || store->kind != StoreKind::TodoItems ||
        store->todo_row_count == std::size(store->todo_rows)) return false;
    store->todo_rows[store->todo_row_count++] = TodoRow{id, description, status};
    return true;
  }

  const InMemoryStore *get(const char *name) const {
    for (std::size_t index = 0; index < count_; ++index) {
      if (std::strcmp(stores_[index].name, name) == 0) return &stores_[index];
    }
    return nullptr;
  }

  const InMemoryStore *get_source(const char *name) const {
    const InMemoryStore *store = get(name);
    return store != nullptr && store->kind == StoreKind::Source ? store : nullptr;
  }

  const InMemoryStore *get_todo_items(const char *name) const {
    const InMemoryStore *store = get(name);
    return store != nullptr && store->kind == StoreKind::TodoItems ? store : nullptr;
  }

  template <typename Visitor>
  void visit(Visitor visitor) const {
    for (std::size_t index = 0; index < count_; ++index) visitor(stores_[index]);
  }

private:
  InMemoryStore *find(const char *name) {
    for (std::size_t index = 0; index < count_; ++index) {
      if (std::strcmp(stores_[index].name, name) == 0) return &stores_[index];
    }
    return nullptr;
  }

  InMemoryStore stores_[StoreCapacity]{};
  std::size_t count_ = 0;
};

struct AppDeclaration {
  char name[32];
  int ideal_width = 0;
  int ideal_height = 0;
  char entry[StoreNameCapacity];
  bool present = false;
};

struct EvalRequest { char expression[ExpressionCapacity]; };
struct EvalResponse { char output[OutputCapacity]; };
enum class StorageRequestKind { BindTodoItems };
struct StorageRequest { StorageRequestKind kind; };
struct StorageCompletion { StorageRequestKind kind; };

InMemoryStoreBackend SourceStores;
AppDeclaration CurrentDeclaration;
bool AppStarted = false;
QueueHandle_t RequestQueue;
QueueHandle_t ResponseQueue;
QueueHandle_t StorageRequestQueue;
QueueHandle_t StorageCompletionQueue;
QueueHandle_t BindReadyQueue;
const InMemoryStore *CurrentSource = nullptr;
std::size_t CurrentRow = 0;
std::size_t CurrentCharacter = 0;
bool SourceAtEnd = false;
const char *ExpressionCursor = nullptr;
char CapturedOutput[OutputCapacity];
std::size_t CapturedOutputLength = 0;
bool CapturingOutput = false;
bool StoreBindStartedPending = false;
bool StoreBindCompletedReady = false;

// Each string below is one in-memory source row.  The app is not called
// directly from C++; boot copies these rows into SourceStores and reads them
// back through the same store lookup path used by the loader.
constexpr const char *TodoManifestRows[] = {
    "#| The bootstrap currently trusts this small manifest to contain only APP-DECLARE. |#",
    "(app-declare :name \"To-do\" :ideal-width 24 :ideal-height 10 :entry \"apps/todo/src/main\")",
};

constexpr const char *TodoMainRows[] = {
    "#| The entry source creates one lexical app instance and then returns. |#",
    "#| APP-START retains the returned event-handler closure for the Shell. |#",
    "(app-start",
    "  (let ((todo-items",
    "#| STORE-BIND returns a live StoreRef immediately, before its rows are ready. |#",
    "         (store-bind \"todo/items\" '(desc status) 0 8)))",
    "#| The handler is deliberately short: one event, one result, then return. |#",
    "    (lambda (event)",
    "      (cond",
    "#| A StoreRef separates its request metadata from its current row collection. |#",
    "        ((eq event 'binding-status)",
    "         (field (field todo-items 'meta) 'status))",
    "#| COUNT reads the live row collection. It is NIL while the bind is pending. |#",
    "        ((eq event 'count)",
    "         (let ((rows (field todo-items 'value)))",
    "           (if rows (length rows) 0)))",
    "#| This observes one bound StoreRowRef's named application field. |#",
    "#| It makes no ordering promise; ordering is a separate future Store API. |#",
    "        ((eq event 'sample-description)",
    "         (let ((rows (field todo-items 'value)))",
    "           (if rows",
    "               (field (field (car rows) 'value) 'desc)",
    "               nil)))",
    "#| Unknown events are returned unchanged while the event API is still small. |#",
    "        (t event)))))",
};

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

void copy_request(EvalRequest &request, const char *expression) {
  std::snprintf(request.expression, sizeof(request.expression), "%s", expression);
}

void seed_boot_source_stores() {
  configASSERT(SourceStores.create_source("apps/todo/app.lisp"));
  for (const char *row : TodoManifestRows) {
    configASSERT(SourceStores.append_source_row("apps/todo/app.lisp", row));
  }
  configASSERT(SourceStores.create_source("apps/todo/src/main"));
  for (const char *row : TodoMainRows) {
    configASSERT(SourceStores.append_source_row("apps/todo/src/main", row));
  }

  // These are ordinary volatile data rows, not a private C++ copy captured by
  // the Lisp app.  The app receives them only through STORE-BIND below.
  configASSERT(SourceStores.create_todo_items("todo/items"));
  configASSERT(SourceStores.append_todo_row(
      "todo/items", 1, "Learn how SilOS loads Lisp from a store", "to do"));
  configASSERT(SourceStores.append_todo_row(
      "todo/items", 2, "Build the first live screen binding", "in progress"));
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
  if (!CapturingOutput) {
    std::putchar(static_cast<unsigned char>(value));
    return;
  }
  if (CapturedOutputLength + 1 < sizeof(CapturedOutput)) CapturedOutput[CapturedOutputLength++] = value;
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
  while (CurrentSource != nullptr && CurrentRow < CurrentSource->source_row_count) {
    const char *row = CurrentSource->source_rows[CurrentRow];
    const char character = row[CurrentCharacter++];
    if (character != '\0') return static_cast<unsigned char>(character);
    ++CurrentRow;
    CurrentCharacter = 0;
    return '\n';
  }
  SourceAtEnd = true;
  return -1;
}

int read_expression_character() {
  if (ExpressionCursor == nullptr || *ExpressionCursor == '\0') return -1;
  return static_cast<unsigned char>(*ExpressionCursor++);
}
}

#include "ulisp-generated.inc"

namespace {
void complete_todo_items_bind() {
  const InMemoryStore *store = SourceStores.get_todo_items("todo/items");
  configASSERT(store != nullptr);
  configASSERT(SilosTodoItemsRef != nil);

  // The storage task never mutates Lisp objects. It sends this bounded
  // completion, and the uLisp task alone installs the live row references.
  object *value = silos_find_field(SilosTodoItemsRef, "value");
  object *metadata = silos_find_field(SilosTodoItemsRef, "meta");
  configASSERT(value != nil);
  configASSERT(metadata != nil);
  object *status = silos_find_field(cdr(metadata), "status");
  configASSERT(status != nil);

  cdr(value) = silos_make_todo_row_refs(*store);
  cdr(status) = silos_symbol("ready");
  StoreBindCompletedReady = true;
  std::puts("store-bind=todo/items status=ready");
  const bool ready = true;
  configASSERT(xQueueSend(BindReadyQueue, &ready, portMAX_DELAY) == pdPASS);
}

void process_storage_completions() {
  StorageCompletion completion{};
  while (xQueueReceive(StorageCompletionQueue, &completion, 0) == pdPASS) {
    if (completion.kind == StorageRequestKind::BindTodoItems) {
      complete_todo_items_bind();
    }
  }
}

void storage_task(void *) {
  for (;;) {
    StorageRequest request{};
    configASSERT(xQueueReceive(StorageRequestQueue, &request, portMAX_DELAY) == pdPASS);
    const StorageCompletion completion{request.kind};
    configASSERT(xQueueSend(StorageCompletionQueue, &completion, portMAX_DELAY) == pdPASS);
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

bool evaluate_expression(const char *expression, char output[OutputCapacity]) {
  CurrentSource = nullptr;
  ExpressionCursor = expression;
  CapturedOutputLength = 0;
  CapturingOutput = true;
  bool succeeded = true;

  if (setjmp(toplevel_handler)) {
    ulisperror();
    succeeded = false;
  } else {
    object *form = readmain(read_expression_character);
    protect(form);
    object *result = eval(form, nullptr);
    printobject(result, pserial);
    unprotect();
    pln(pserial);
  }

  CapturedOutput[CapturedOutputLength] = '\0';
  std::snprintf(output, OutputCapacity, "%s", CapturedOutput);
  CapturingOutput = false;
  return succeeded;
}

bool bootstrap_apps() {
  bool found_manifest = false;
  bool loaded = true;
  SourceStores.visit([&](const InMemoryStore &store) {
    if (!is_app_manifest_name(store.name)) return;
    found_manifest = true;
    CurrentDeclaration = AppDeclaration{};
    AppStarted = false;
    loaded = evaluate_source_store(store) && CurrentDeclaration.present && loaded;
    const InMemoryStore *entry = SourceStores.get_source(CurrentDeclaration.entry);
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
    EvalRequest request{};
    EvalResponse response{};
    // One whole tick gives the cooperative Browser scheduler a yield point.
    // pdMS_TO_TICKS(1) rounds down to zero at this target's 100 Hz tick rate.
    if (xQueueReceive(RequestQueue, &request, 1) == pdPASS) {
      (void)evaluate_expression(request.expression, response.output);
      configASSERT(xQueueSend(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
    }
  }
}

bool expect_evaluation(const char *expression, const char *expected_output) {
  EvalRequest request{};
  EvalResponse response{};
  copy_request(request, expression);
  configASSERT(xQueueSend(RequestQueue, &request, portMAX_DELAY) == pdPASS);
  configASSERT(xQueueReceive(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
  const bool matches = std::strcmp(response.output, expected_output) == 0;
  std::printf("eval=%s result=%s match=%s", expression, response.output,
              matches ? "yes\n" : "no\n");
  return matches;
}

void client_task(void *) {
  // Wait for the uLisp task's completion notification, rather than assuming a
  // duration. This makes the pending-to-ready proof deterministic.
  bool bind_ready = false;
  configASSERT(xQueueReceive(BindReadyQueue, &bind_ready, portMAX_DELAY) == pdPASS);
  bool passed = bind_ready && StoreBindStartedPending && StoreBindCompletedReady;
  passed = expect_evaluation("(silos-test-event 'binding-status)", "ready\r\n") && passed;
  passed = expect_evaluation("(silos-test-event 'count)", "2\r\n") && passed;
  passed = expect_evaluation("(silos-test-event 'sample-description)",
                             "\"Learn how SilOS loads Lisp from a store\"\r\n") && passed;
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
  seed_boot_source_stores();
  RequestQueue = xQueueCreate(1, sizeof(EvalRequest));
  ResponseQueue = xQueueCreate(1, sizeof(EvalResponse));
  StorageRequestQueue = xQueueCreate(1, sizeof(StorageRequest));
  StorageCompletionQueue = xQueueCreate(1, sizeof(StorageCompletion));
  BindReadyQueue = xQueueCreate(1, sizeof(bool));
  configASSERT(RequestQueue != nullptr);
  configASSERT(ResponseQueue != nullptr);
  configASSERT(StorageRequestQueue != nullptr);
  configASSERT(StorageCompletionQueue != nullptr);
  configASSERT(BindReadyQueue != nullptr);
  configASSERT(xTaskCreate(ulisp_task, "ulisp", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(storage_task, "store", ULispTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  configASSERT(xTaskCreate(client_task, "client", ClientTaskStackBytes, nullptr, 2, nullptr) == pdPASS);
  vTaskStartScheduler();
  return 0;
}
