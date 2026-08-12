#include "arduino_compat.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <emscripten.h>
#include <emscripten/heap.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#if defined(FREEWISP_WORKER_UI)
EM_JS(int, freewisp_worker_receive_control, (), {
  if (!globalThis.freewispControls || globalThis.freewispControls.length === 0) return 0;
  const control = globalThis.freewispControls.shift();
  if (control === 'pause') return 1;
  if (control === 'resume') return 2;
  if (control === 'step') return 3;
  return 0;
});

EM_JS(int, freewisp_worker_receive_request,
      (char *expression, int capacity, int *request_id), {
  if (!globalThis.freewispRequests || globalThis.freewispRequests.length === 0) return 0;
  const request = globalThis.freewispRequests.shift();
  stringToUTF8(request.expression, expression, capacity);
  HEAP32[request_id >> 2] = request.id;
  return 1;
});

EM_JS(void, freewisp_worker_post_ready, (), {
  postMessage({ type: 'ready' });
});

EM_JS(void, freewisp_worker_post_status, (int paused, unsigned long tick), {
  postMessage({ type: 'status', paused: !!paused, tick });
});

EM_JS(void, freewisp_worker_post_display,
      (const std::uint8_t *framebuffer, size_t size, unsigned long revision), {
  const pixels = HEAPU8.slice(framebuffer, framebuffer + size);
  postMessage({ type: 'display', revision, pixels }, [pixels.buffer]);
});

EM_JS(void, freewisp_worker_post_result,
      (int request_id, const char *output, unsigned long yields,
       unsigned long start_tick, unsigned long end_tick, double elapsed_ms,
       size_t free_heap,
       size_t minimum_free_heap, double max_uninterrupted_ms,
       unsigned long intervals_over_100_ms,
       unsigned long intervals_over_250_ms, unsigned long gc_count,
       double gc_total_ms, double gc_max_ms, double max_tick_lateness_ms,
       unsigned long scheduler_passes, unsigned long tick_catchup_events,
       unsigned long max_ticks_per_pass, size_t heap_capacity,
       size_t ulisp_workspace, size_t ulisp_free, size_t wasm_linear_memory,
       size_t wasm_dynamic_top), {
  postMessage({
    type: 'result',
    id: request_id,
    output: UTF8ToString(output),
    safePointYields: yields,
    startTick: start_tick,
    tick: end_tick,
    tickDelta: end_tick - start_tick,
    elapsedMs: elapsed_ms,
    freeHeapBytes: free_heap,
    minimumFreeHeapBytes: minimum_free_heap,
    freeRtosHeapCapacityBytes: heap_capacity,
    ulispWorkspaceBytes: ulisp_workspace,
    ulispFreeBytes: ulisp_free,
    wasmMemoryBytes: wasm_linear_memory,
    wasmDynamicTopBytes: wasm_dynamic_top,
    maxUninterruptedMs: max_uninterrupted_ms,
    intervalsOver100Ms: intervals_over_100_ms,
    intervalsOver250Ms: intervals_over_250_ms,
    gcCount: gc_count,
    gcTotalMs: gc_total_ms,
    gcMaxMs: gc_max_ms,
    maxTickLatenessMs: max_tick_lateness_ms,
    schedulerPasses: scheduler_passes,
    tickCatchupEvents: tick_catchup_events,
    maxTicksPerPass: max_ticks_per_pass
  });
});
#endif

HardwareSerial Serial;
HardwareSerial Serial1;
SPIClass SPI;
TwoWire Wire;
TwoWire Wire1;
LittleFSClass LittleFS;
WiFiClass WiFi;

namespace {
constexpr double YieldBudgetMilliseconds = 5.0;
constexpr std::size_t ExpressionCapacity = 256;
constexpr std::size_t OutputCapacity = 2048;
constexpr std::size_t DisplayWidth = 128;
constexpr std::size_t DisplayHeight = 64;
constexpr std::size_t DisplayBytes = DisplayWidth * DisplayHeight / 8;
constexpr configSTACK_DEPTH_TYPE ULispTaskStackBytes = 65536U;
constexpr configSTACK_DEPTH_TYPE HelperTaskStackBytes = 16384U;

struct EvalRequest {
  char expression[ExpressionCapacity];
};

struct EvalResponse {
  char output[OutputCapacity];
  std::uint32_t safe_point_yields;
  double max_uninterrupted_ms;
  std::uint32_t intervals_over_100_ms;
  std::uint32_t intervals_over_250_ms;
  std::uint32_t gc_count;
  double gc_total_ms;
  double gc_max_ms;
  FreeWispPortStats port_stats;
};

QueueHandle_t RequestQueue;
QueueHandle_t ResponseQueue;
const char *InputCursor = nullptr;
char Output[OutputCapacity];
std::size_t OutputLength = 0;
bool CaptureOutput = false;
double YieldDeadline = 0.0;
std::uint32_t SafePointYields = 0;
double LastSafePointTime = 0.0;
double MaxUninterruptedMilliseconds = 0.0;
std::uint32_t IntervalsOver100Milliseconds = 0;
std::uint32_t IntervalsOver250Milliseconds = 0;
double GarbageCollectionStarted = 0.0;
double GarbageCollectionTotalMilliseconds = 0.0;
double GarbageCollectionMaxMilliseconds = 0.0;
std::uint32_t GarbageCollectionCount = 0;
std::uint8_t DisplayFramebuffer[DisplayBytes]{};
std::uint32_t DisplayRevision = 0;
#if !defined(FREEWISP_WORKER_UI)
std::uint32_t ObserverRuns = 0;
#endif

int read_expression_character() {
  if (InputCursor == nullptr || *InputCursor == '\0') return '\n';
  return static_cast<unsigned char>(*InputCursor++);
}

void record_uninterrupted_interval(double now) {
  const double elapsed = now - LastSafePointTime;
  MaxUninterruptedMilliseconds = std::max(MaxUninterruptedMilliseconds, elapsed);
  if (elapsed > 100.0) ++IntervalsOver100Milliseconds;
  if (elapsed > 250.0) ++IntervalsOver250Milliseconds;
  LastSafePointTime = now;
}

#if !defined(FREEWISP_WORKER_UI)
void set_request(EvalRequest &request, const char *expression) {
  std::snprintf(request.expression, sizeof(request.expression), "%s", expression);
}
#endif
}

void freewisp_serial_write(char value) {
  if (!CaptureOutput) {
    std::putchar(static_cast<unsigned char>(value));
    return;
  }
  if (OutputLength + 1 < OutputCapacity) Output[OutputLength++] = value;
}

unsigned long millis() {
  return static_cast<unsigned long>(emscripten_get_now());
}

unsigned long micros() {
  return static_cast<unsigned long>(emscripten_get_now() * 1000.0);
}

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

void freewisp_ulisp_safe_point() {
  const double now = emscripten_get_now();
  record_uninterrupted_interval(now);
  if (now < YieldDeadline) return;
  ++SafePointYields;
  taskYIELD();
  LastSafePointTime = emscripten_get_now();
  YieldDeadline = LastSafePointTime + YieldBudgetMilliseconds;
}

void freewisp_gc_started() {
  GarbageCollectionStarted = emscripten_get_now();
}

void freewisp_gc_finished() {
  const double elapsed = emscripten_get_now() - GarbageCollectionStarted;
  ++GarbageCollectionCount;
  GarbageCollectionTotalMilliseconds += elapsed;
  GarbageCollectionMaxMilliseconds = std::max(GarbageCollectionMaxMilliseconds, elapsed);
}

void freewisp_display_clear(bool on) {
  std::memset(DisplayFramebuffer, on ? 0xFF : 0x00, sizeof(DisplayFramebuffer));
  ++DisplayRevision;
}

void freewisp_display_pixel(int x, int y, bool on) {
  if (x < 0 || x >= static_cast<int>(DisplayWidth) ||
      y < 0 || y >= static_cast<int>(DisplayHeight)) {
    return;
  }
  const std::size_t bit = static_cast<std::size_t>(y) * DisplayWidth +
                          static_cast<std::size_t>(x);
  const std::uint8_t mask = static_cast<std::uint8_t>(1U << (bit & 7U));
  std::uint8_t &value = DisplayFramebuffer[bit >> 3U];
  const std::uint8_t updated = on ? static_cast<std::uint8_t>(value | mask)
                                  : static_cast<std::uint8_t>(value & ~mask);
  if (updated == value) return;
  value = updated;
  ++DisplayRevision;
}

#include "ulisp-generated.inc"

namespace {
void evaluate(const EvalRequest &request, EvalResponse &response) {
  const std::uint32_t starting_yields = SafePointYields;

  OutputLength = 0;
  CaptureOutput = true;
  LastSafePointTime = emscripten_get_now();
  YieldDeadline = LastSafePointTime + YieldBudgetMilliseconds;
  MaxUninterruptedMilliseconds = 0.0;
  IntervalsOver100Milliseconds = 0;
  IntervalsOver250Milliseconds = 0;
  GarbageCollectionCount = 0;
  GarbageCollectionTotalMilliseconds = 0.0;
  GarbageCollectionMaxMilliseconds = 0.0;
  vPortResetStats();

  if (setjmp(toplevel_handler)) {
    ulisperror();
  } else {
    InputCursor = request.expression;
    object *form = readmain(read_expression_character);
    protect(form);
    object *result = eval(form, nullptr);
    printobject(result, pserial);
    unprotect();
    pln(pserial);
  }

  record_uninterrupted_interval(emscripten_get_now());

  Output[OutputLength] = '\0';
  CaptureOutput = false;
  std::snprintf(response.output, sizeof(response.output), "%s", Output);
  response.safe_point_yields = SafePointYields - starting_yields;
  response.max_uninterrupted_ms = MaxUninterruptedMilliseconds;
  response.intervals_over_100_ms = IntervalsOver100Milliseconds;
  response.intervals_over_250_ms = IntervalsOver250Milliseconds;
  response.gc_count = GarbageCollectionCount;
  response.gc_total_ms = GarbageCollectionTotalMilliseconds;
  response.gc_max_ms = GarbageCollectionMaxMilliseconds;
  vPortGetStats(&response.port_stats);
}

void ulisp_task(void *) {
  EvalRequest request{};
  EvalResponse response{};

  setup();
#if defined(FREEWISP_WORKER_UI)
  freewisp_worker_post_ready();
#endif
  for (;;) {
    configASSERT(xQueueReceive(RequestQueue, &request, portMAX_DELAY) == pdPASS);
    evaluate(request, response);
    configASSERT(xQueueSend(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
  }
}

#if defined(FREEWISP_WORKER_UI)
void worker_client_task(void *) {
  EvalRequest request{};
  EvalResponse response{};
  bool paused = false;
  bool step_requested = false;
  int request_id = 0;
  std::uint32_t posted_display_revision = DisplayRevision;

  freewisp_worker_post_status(paused, xTaskGetTickCount());
  for (;;) {
    switch (freewisp_worker_receive_control()) {
      case 1:
        paused = true;
        step_requested = false;
        freewisp_worker_post_status(paused, xTaskGetTickCount());
        break;
      case 2:
        paused = false;
        step_requested = false;
        freewisp_worker_post_status(paused, xTaskGetTickCount());
        break;
      case 3:
        if (paused) step_requested = true;
        freewisp_worker_post_status(paused, xTaskGetTickCount());
        break;
      default:
        break;
    }

    if ((!paused || step_requested) &&
        freewisp_worker_receive_request(request.expression,
                                        static_cast<int>(sizeof(request.expression)),
                                        &request_id)) {
      const double started = emscripten_get_now();
      const TickType_t start_tick = xTaskGetTickCount();
      configASSERT(xQueueSend(RequestQueue, &request, portMAX_DELAY) == pdPASS);
      configASSERT(xQueueReceive(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
      freewisp_worker_post_result(request_id,
                                  response.output,
                                  response.safe_point_yields,
                                  start_tick,
                                  xTaskGetTickCount(),
                                  emscripten_get_now() - started,
                                  xPortGetFreeHeapSize(),
                                  xPortGetMinimumEverFreeHeapSize(),
                                  response.max_uninterrupted_ms,
                                  response.intervals_over_100_ms,
                                  response.intervals_over_250_ms,
                                  response.gc_count,
                                  response.gc_total_ms,
                                  response.gc_max_ms,
                                  response.port_stats.max_tick_lateness_ms,
                                  response.port_stats.scheduler_passes,
                                  response.port_stats.tick_catchup_events,
                                  response.port_stats.max_ticks_per_pass,
                                  configTOTAL_HEAP_SIZE,
                                  sizeof(Workspace),
                                  static_cast<size_t>(Freespace) * sizeof(object),
                                  emscripten_get_heap_size(),
                                  *emscripten_get_sbrk_ptr());
      if (DisplayRevision != posted_display_revision) {
        freewisp_worker_post_display(DisplayFramebuffer,
                                     sizeof(DisplayFramebuffer),
                                     DisplayRevision);
        posted_display_revision = DisplayRevision;
      }
      step_requested = false;
      freewisp_worker_post_status(paused, xTaskGetTickCount());
    }

    vTaskDelay(1);
  }
}
#else
void observer_task(void *) {
  for (;;) {
    ++ObserverRuns;
    taskYIELD();
  }
}

void client_task(void *) {
  static const char *expressions[] = {
    "(defvar answer 40)",
    "(+ answer 2)",
    "(let ((x 0)) (dotimes (i 100000 x) (setq x (+ x 1))))",
    "(progn (display-clear) (display-pixel 7 9 t))"
  };
  static const char *expected[] = { "answer\r\n", "42\r\n", "100000\r\n", "t\r\n" };
  EvalRequest request{};
  EvalResponse response{};
  std::uint32_t long_eval_observer_start = 0;
  bool passed = true;

  for (std::size_t index = 0; index < std::size(expressions); ++index) {
    bool output_matches;

    set_request(request, expressions[index]);
    if (index == 2) long_eval_observer_start = ObserverRuns;
    configASSERT(xQueueSend(RequestQueue, &request, portMAX_DELAY) == pdPASS);
    configASSERT(xQueueReceive(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
    output_matches = std::strcmp(response.output, expected[index]) == 0;
    passed = passed && output_matches;

    std::printf("eval[%zu]=%s yields=%lu match=%s length=%zu\n",
                index,
                response.output,
                static_cast<unsigned long>(response.safe_point_yields),
                output_matches ? "yes" : "no",
                std::strlen(response.output));

    if (index == 2) {
      const bool yielded = response.safe_point_yields > 0;
      const bool observer_ran = ObserverRuns > long_eval_observer_start;
      passed = passed && yielded && observer_ran;
      std::printf("long_eval_yielded=%s observer_ran=%s observer_delta=%lu\n",
                  yielded ? "yes" : "no",
                  observer_ran ? "yes" : "no",
                  static_cast<unsigned long>(ObserverRuns - long_eval_observer_start));
    }
    if (index == 3) {
      const std::size_t bit = 9U * DisplayWidth + 7U;
      const bool pixel_set = (DisplayFramebuffer[bit >> 3U] &
                              static_cast<std::uint8_t>(1U << (bit & 7U))) != 0U;
      passed = passed && pixel_set;
      std::printf("display_pixel_set=%s revision=%lu\n",
                  pixel_set ? "yes" : "no",
                  static_cast<unsigned long>(DisplayRevision));
    }
  }

  std::printf("observer_runs=%lu\n", static_cast<unsigned long>(ObserverRuns));
  std::puts(passed ? "FREEWISP_ULISP_TASK_PASS" : "FREEWISP_ULISP_TASK_FAIL");
  if (!passed) std::abort();

  vTaskEndScheduler();
  std::abort();
}
#endif
}

extern "C" void vFreeWispAssert(const char *file, int line) {
  std::fprintf(stderr, "FreeWisp assertion failed at %s:%d\n", file, line);
  std::abort();
}

extern "C" void vApplicationMallocFailedHook() {
  vFreeWispAssert("FreeRTOS heap exhausted", 0);
}

extern "C" void vApplicationIdleHook() {
  vPortWaitForTick();
}

int main() {
  RequestQueue = xQueueCreate(1, sizeof(EvalRequest));
  ResponseQueue = xQueueCreate(1, sizeof(EvalResponse));
  configASSERT(RequestQueue != nullptr);
  configASSERT(ResponseQueue != nullptr);

  configASSERT(xTaskCreate(ulisp_task,
                           "ulisp",
                           ULispTaskStackBytes,
                           nullptr,
                           2,
                           nullptr) == pdPASS);
#if defined(FREEWISP_WORKER_UI)
  configASSERT(xTaskCreate(worker_client_task,
                           "worker-client",
                           HelperTaskStackBytes,
                           nullptr,
                           2,
                           nullptr) == pdPASS);
#else
  configASSERT(xTaskCreate(client_task,
                           "client",
                           HelperTaskStackBytes,
                           nullptr,
                           2,
                           nullptr) == pdPASS);
  configASSERT(xTaskCreate(observer_task,
                           "observer",
                           HelperTaskStackBytes,
                           nullptr,
                           2,
                           nullptr) == pdPASS);
#endif

  vTaskStartScheduler();
  return 0;
}
