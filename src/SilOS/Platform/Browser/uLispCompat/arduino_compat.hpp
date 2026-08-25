#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define ESP32 1
#define PROGMEM
#define PSTR(value) (value)
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
#define LSBFIRST 0
#define MSBFIRST 1
#define WL_CONNECTED 3
#define WL_NO_SSID_AVAIL 1
#define WL_CONNECT_FAILED 4
#define WL_DISCONNECTED 6
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

using byte = std::uint8_t;

void freewisp_serial_write(char value);

unsigned long millis();
unsigned long micros();
void delay(unsigned long milliseconds);
void randomSeed(unsigned long seed);
long random(long upper);
long random(long lower, long upper);
void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
int analogRead(int pin);
void analogWrite(int pin, int value);
void analogReadResolution(int bits);
void analogWriteResolution(int bits);
void dacWrite(int pin, int value);
void yield();
void esp_light_sleep_start();
void esp_sleep_enable_timer_wakeup(std::uint64_t microseconds);
void configTime(long offset_seconds, int daylight_offset_seconds, const char *server);

class HardwareSerial {
public:
  void begin(long) {}
  void end() {}
  void flush() { std::fflush(stdout); }
  int available() { return 0; }
  int read() { return -1; }
  std::size_t write(char value) {
    freewisp_serial_write(value);
    return 1;
  }
  std::size_t write(std::uint8_t value) { return write(static_cast<char>(value)); }
  void print(const char *value) { std::fputs(value, stdout); }
  explicit operator bool() const { return true; }
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;

class SPISettings {
public:
  SPISettings(unsigned long, int, int) {}
};

class SPIClass {
public:
  explicit SPIClass(int = 0) {}
  void begin() {}
  void end() {}
  void beginTransaction(const SPISettings &) {}
  void endTransaction() {}
  std::uint8_t transfer(std::uint8_t value) { return value; }
};

extern SPIClass SPI;

class TwoWire {
public:
  void begin() {}
  void end() {}
  void beginTransmission(int) {}
  int endTransmission(bool = true) { return 0; }
  std::size_t write(std::uint8_t) { return 1; }
  int requestFrom(int, int count) { return count; }
  int available() { return 0; }
  int read() { return -1; }
};

extern TwoWire Wire;
extern TwoWire Wire1;

class File {
public:
  explicit operator bool() const { return false; }
  int read() { return -1; }
  int read(std::uint8_t *, std::size_t) { return 0; }
  std::size_t write(std::uint8_t) { return 0; }
  std::size_t write(const std::uint8_t *, std::size_t size) { return size; }
  void close() {}
};

class LittleFSClass {
public:
  bool begin(bool = false) { return true; }
  void end() {}
  File open(const char *, const char * = nullptr) { return File{}; }
  bool remove(const char *) { return false; }
  std::size_t totalBytes() const { return 0; }
};

extern LittleFSClass LittleFS;

class WiFiClient {
public:
  explicit operator bool() const { return false; }
  bool connect(const char *, std::uint16_t) { return false; }
  bool connect(std::uint32_t, std::uint16_t) { return false; }
  int available() { return 0; }
  int read() { return -1; }
  std::size_t write(char) { return 0; }
  bool connected() const { return false; }
  void stop() {}
};

class WiFiServer {
public:
  explicit WiFiServer(std::uint16_t) {}
  void begin() {}
  WiFiClient available() { return WiFiClient{}; }
};

class IPAddress {
public:
  std::uint8_t operator[](int) const { return 0; }
  operator std::uint32_t() const { return 0; }
};

class WiFiClass {
public:
  bool softAPdisconnect(bool) { return true; }
  bool softAP(const char *, const char * = nullptr, int = 1, bool = false) { return false; }
  IPAddress softAPIP() const { return IPAddress{}; }
  IPAddress localIP() const { return IPAddress{}; }
  void disconnect(bool = false) {}
  void begin(const char *, const char * = nullptr) {}
  int waitForConnectResult() const { return WL_CONNECT_FAILED; }
};

extern WiFiClass WiFi;
