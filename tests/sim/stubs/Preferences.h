#pragma once
struct Preferences {
  void begin(const char*, bool) {}
  size_t getBytes(const char*, void* p, size_t n) { memset(p, 0, n); return n; }
  void putBytes(const char*, const void*, size_t) {}
  uint8_t getUChar(const char*, uint8_t d) { return d; }
  void putUChar(const char* k, uint8_t v) { fprintf(stderr, "[%6u] saved %s=%d\n", millis(), k, v); }
  void end() {}
};
