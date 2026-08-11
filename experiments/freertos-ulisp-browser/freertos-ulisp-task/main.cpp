#include "arduino_compat.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
constexpr double YieldBudgetMilliseconds = 5.0;
constexpr std::size_t ExpressionCapacity = 256;
constexpr std::size_t OutputCapacity = 2048;
constexpr configSTACK_DEPTH_TYPE ULispTaskStackBytes = 65536U;
constexpr configSTACK_DEPTH_TYPE HelperTaskStackBytes = 16384U;

struct EvalRequest {
  char expression[ExpressionCapacity];
};

struct EvalResponse {
  char output[OutputCapacity];
  std::uint32_t safe_point_yields;
};

QueueHandle_t RequestQueue;
QueueHandle_t ResponseQueue;
const char *InputCursor = nullptr;
char Output[OutputCapacity];
std::size_t OutputLength = 0;
bool CaptureOutput = false;
double YieldDeadline = 0.0;
std::uint32_t SafePointYields = 0;
std::uint32_t ObserverRuns = 0;

int read_expression_character() {
  if (InputCursor == nullptr || *InputCursor == '\0') return '\n';
  return static_cast<unsigned char>(*InputCursor++);
}

void set_request(EvalRequest &request, const char *expression) {
  std::snprintf(request.expression, sizeof(request.expression), "%s", expression);
}
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
  if (emscripten_get_now() < YieldDeadline) return;
  ++SafePointYields;
  taskYIELD();
  YieldDeadline = emscripten_get_now() + YieldBudgetMilliseconds;
}

#include "ulisp-generated.inc"

namespace {
void evaluate(const EvalRequest &request, EvalResponse &response) {
  const std::uint32_t starting_yields = SafePointYields;

  OutputLength = 0;
  CaptureOutput = true;
  YieldDeadline = emscripten_get_now() + YieldBudgetMilliseconds;

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

  Output[OutputLength] = '\0';
  CaptureOutput = false;
  std::snprintf(response.output, sizeof(response.output), "%s", Output);
  response.safe_point_yields = SafePointYields - starting_yields;
}

void ulisp_task(void *) {
  EvalRequest request{};
  EvalResponse response{};

  setup();
  for (;;) {
    configASSERT(xQueueReceive(RequestQueue, &request, portMAX_DELAY) == pdPASS);
    evaluate(request, response);
    configASSERT(xQueueSend(ResponseQueue, &response, portMAX_DELAY) == pdPASS);
  }
}

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
    "(let ((x 0)) (dotimes (i 100000 x) (setq x (+ x 1))))"
  };
  static const char *expected[] = { "answer\r\n", "42\r\n", "100000\r\n" };
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
  }

  std::printf("observer_runs=%lu\n", static_cast<unsigned long>(ObserverRuns));
  std::puts(passed ? "FREEWISP_ULISP_TASK_PASS" : "FREEWISP_ULISP_TASK_FAIL");
  if (!passed) std::abort();

  vTaskEndScheduler();
  std::abort();
}
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

  vTaskStartScheduler();
  return 0;
}
