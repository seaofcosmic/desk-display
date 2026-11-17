// protocol.h - USB serial framing between the companion app and the display.
// Plain C++ with no Arduino dependencies so it can be tested on a computer.
//
// Computer -> display (binary frames):
//   'D' 'T' <type:1> <len:4, little-endian> <payload:len> <checksum:2, little-endian>
//   checksum = sum of all payload bytes, modulo 65536
//
//   type 'H'  hello            (no payload)      display answers "DT:READY:1"
//   type 'M'  track info       "title\nartist\nalbum" (ASCII)
//   type 'S'  playback state   <flags:1> <position ms:4> <duration ms:4>
//                              flags bit0 = playing, bit1 = position known, bit2 = can seek
//   type 'A'  album art        140 x 140 pixels, RGB565, high byte first (39,200 bytes)
//   type 'X'  nothing playing  (no payload)
//   type 'T'  clock            <local time, seconds since 1970:4> <unused:1>
//   type 'W'  weather          text lines, see parseWeather() in apps.h
//   type 'R'  reminders        text lines, see parseReminders() in apps.h
//   type 'N'  alert            "time\ntext"   (a timed reminder or an upcoming calendar event)
//   type 'E'  calendar         text lines, see parseEvents() in apps.h
//   type 'Y'  computer stats   text lines, see parseStats() in apps.h
//
// Display -> computer (text lines):
//   DT:READY:1   DT:CMD:PLAY   DT:CMD:NEXT   DT:CMD:PREV   DT:CMD:SEEK:<ms>   DT:NAK:A
//   DT:CMD:REM:<n>   (reminder n was tapped)
//   DT:SET:H24=0,WC=0,RA=1,CA=1   (the display's settings that the computer needs to follow)
#pragma once
#include <stdint.h>
#include <stddef.h>

static const int      ART_SIZE       = 140;
static const uint32_t ART_BYTES      = (uint32_t)ART_SIZE * ART_SIZE * 2;
static const uint32_t SMALL_MAX      = 1024;

static const uint8_t  STATE_PLAYING  = 0x01;
static const uint8_t  STATE_HAS_POS  = 0x02;
static const uint8_t  STATE_CAN_SEEK = 0x04;

class FrameParser {
 public:
  // artBuffer must hold ART_BYTES bytes. Album art frames are written straight into it.
  explicit FrameParser(uint8_t* artBuffer) : _art(artBuffer) { reset(); }

  void reset() { _st = S_MAGIC1; _got = 0; _len = 0; _sum = 0; _type = 0; }
  bool midFrame() const { return _st != S_MAGIC1; }

  // Feed one byte. Returns true when a complete frame has just arrived;
  // then read type(), payload(), length() and ok().
  bool feed(uint8_t b) {
    switch (_st) {
      case S_MAGIC1:
        if (b == 'D') _st = S_MAGIC2;
        return false;
      case S_MAGIC2:
        _st = (b == 'T') ? S_TYPE : (b == 'D' ? S_MAGIC2 : S_MAGIC1);
        return false;
      case S_TYPE:
        _type = b; _got = 0; _len = 0;
        _st = S_LEN;
        return false;
      case S_LEN:
        _len |= ((uint32_t)b) << (8 * _got);
        if (++_got == 4) {
          bool sizeOk = (_type == 'A') ? (_len == ART_BYTES) : (_len <= SMALL_MAX);
          if (!sizeOk) { reset(); return false; }   // not a real frame, wait for the next one
          _got = 0; _sum = 0; _rxSum = 0;
          _st = (_len == 0) ? S_SUM : S_PAYLOAD;
        }
        return false;
      case S_PAYLOAD:
        if (_type == 'A') _art[_got] = b; else _small[_got] = b;
        _sum = (uint16_t)(_sum + b);
        if (++_got == _len) { _got = 0; _rxSum = 0; _st = S_SUM; }
        return false;
      case S_SUM:
        _rxSum |= ((uint16_t)b) << (8 * _got);
        if (++_got == 2) {
          _ok = (_rxSum == _sum);
          if (_type != 'A') _small[_len] = 0;     // make text payloads usable as C strings
          _st = S_MAGIC1;
          _doneType = _type; _doneLen = _len;
          return true;
        }
        return false;
    }
    return false;
  }

  uint8_t        type()    const { return _doneType; }
  uint32_t       length()  const { return _doneLen; }
  bool           ok()      const { return _ok; }
  const uint8_t* payload() const { return (_doneType == 'A') ? _art : _small; }

 private:
  enum St { S_MAGIC1, S_MAGIC2, S_TYPE, S_LEN, S_PAYLOAD, S_SUM };
  St       _st;
  uint8_t  _type, _doneType = 0;
  uint32_t _len, _got, _doneLen = 0;
  uint16_t _sum, _rxSum = 0;
  bool     _ok = false;
  uint8_t* _art;
  uint8_t  _small[SMALL_MAX + 1];
};

static inline uint32_t readU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
