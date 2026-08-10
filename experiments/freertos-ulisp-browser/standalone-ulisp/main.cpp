#include "arduino_compat.hpp"

#include <chrono>
#include <cstring>
#include <emscripten/emscripten.h>

HardwareSerial Serial;
HardwareSerial Serial1;
SPIClass SPI;
TwoWire Wire;
TwoWire Wire1;
LittleFSClass LittleFS;
WiFiClass WiFi;

namespace {
const auto StartedAt = std::chrono::steady_clock::now();
const char *InputCursor = nullptr;
constexpr std::size_t OutputCapacity = 8192;
char Output[OutputCapacity];
std::size_t OutputLength = 0;
bool CaptureOutput = false;
bool Initialized = false;

int read_expression_character() {
  if (InputCursor == nullptr || *InputCursor == '\0') return '\n';
  return static_cast<unsigned char>(*InputCursor++);
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
  const auto elapsed = std::chrono::steady_clock::now() - StartedAt;
  return static_cast<unsigned long>(
    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

unsigned long micros() {
  const auto elapsed = std::chrono::steady_clock::now() - StartedAt;
  return static_cast<unsigned long>(
    std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

void delay(unsigned long) {}
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
void yield() {}
void esp_light_sleep_start() {}
void esp_sleep_enable_timer_wakeup(std::uint64_t) {}
void configTime(long, int, const char *) {}

#include "ulisp-generated.inc"

namespace {
void initialize_ulisp() {
  if (Initialized) return;
  setup();
  Initialized = true;
}

const char *finish_output() {
  Output[OutputLength] = '\0';
  CaptureOutput = false;
  return Output;
}
}

extern "C" EMSCRIPTEN_KEEPALIVE const char *freewisp_eval(const char *expression) {
  initialize_ulisp();
  OutputLength = 0;
  CaptureOutput = true;

  if (setjmp(toplevel_handler)) {
    ulisperror();
    return finish_output();
  }

  InputCursor = expression == nullptr ? "nil" : expression;
  object *form = readmain(read_expression_character);
  protect(form);
  object *result = eval(form, nullptr);
  printobject(result, pserial);
  unprotect();
  pln(pserial);
  return finish_output();
}

int main(int argc, char **argv) {
  if (argc == 1) {
    std::fputs(freewisp_eval("(+ 20 22)"), stdout);
    return 0;
  }
  for (int i = 1; i < argc; ++i) std::fputs(freewisp_eval(argv[i]), stdout);
  return 0;
}
