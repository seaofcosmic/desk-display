// Simulator stand-in for the real screen: draws into memory, touches come from a script.
#pragma once
#include <LovyanGFX.hpp>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <sys/ioctl.h>
static uint32_t millis() {
  static auto t0 = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}
static void delay(uint32_t ms) { usleep(ms * 1000); }
#define OUTPUT 1
#define HIGH 1
#define LOW 0
static void pinMode(int, int) {}
static int lightChanges = 0;
static uint32_t millis();
static void digitalWrite(int pin, int v) { if (pin == 4 && ++lightChanges <= 4) fprintf(stderr, "[light] red %s\n", v ? "off" : "ON"); }
static float temperatureRead() { return 47.3f; }
struct SimEsp { uint32_t getFreeHeap() { return 217 * 1024; } };
static SimEsp ESP;
struct SimSerial {
  int fd = -1;
  void setRxBufferSize(int) {}
  void begin(int) {}
  int available() { int n = 0; ioctl(fd, FIONREAD, &n); return n; }
  int readBytes(uint8_t* b, int n) { return (int)read(fd, b, n); }
  void print(const char* s) { fprintf(stderr, "[%6u] device -> %s", millis(), s[0] == '\n' ? s + 1 : s); if (write(fd, s, strlen(s)) < 0) {} }
  void printf(const char* fmt, ...) { char b[128]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); print(b); }
};
static SimSerial Serial;
struct Touch { uint32_t t0, t1; int x0, y0, x1, y1; };
static std::vector<Touch> touchScript;
class LGFX : public LGFX_Sprite {
 public:
  bool init() { setColorDepth(16); createSprite(320, 240); return true; }
  void setRotation(int) {}
  void setBrightness(int) {}
  uint_fast8_t getTouch(int32_t* x, int32_t* y) {
    uint32_t t = millis();
    for (auto& s : touchScript) if (t >= s.t0 && t <= s.t1) {
      float f = s.t1 > s.t0 ? (float)(t - s.t0) / (s.t1 - s.t0) : 0;
      *x = s.x0 + (int)((s.x1 - s.x0) * f); *y = s.y0 + (int)((s.y1 - s.y0) * f);
      return 1;
    }
    return 0;
  }
  uint_fast8_t getTouchRaw(lgfx::touch_point_t*) { return 0; }
  template <typename A, typename B> void calibrateTouch(uint16_t*, A, B, int) {}
  void setTouchCalibrate(uint16_t*) {}
};
