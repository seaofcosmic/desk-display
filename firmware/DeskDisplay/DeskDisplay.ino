// DeskDisplay - a now-playing screen for the ESP32 "Cheap Yellow Display" (ESP32-2432S028R).
//
// The board shows whatever the companion app on the computer sends over the USB cable:
// album art, title, artist, a scrub gauge and prev / play-pause / next buttons.
// Touches are sent back to the computer, which controls the music app.
//
// Arduino IDE setup:
//   Board:   "ESP32 Dev Module"      (install "esp32 by Espressif Systems" in Boards Manager)
//   Library: "LovyanGFX"             (install from Library Manager)
//
// Look and layout live in ui.h. The USB message format lives in protocol.h.

// ---------------------------------------------------------------- SETTINGS
// Which screen chip the board has.
//   0 = detect automatically (works for both the one-port and two-port boards)
//   1 = force ILI9341 (usually the board with one USB port)
//   2 = force ST7789  (usually the board with two USB ports)
#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER 0
#endif

// 1 or 3. Change this if the picture is upside down for the way your board sits.
#define SCREEN_ROTATION 1

// Must match the companion app.
#define SERIAL_BAUD 460800

// ---------------------------------------------------------------- DISPLAY SETUP
#if DISPLAY_DRIVER == 0

  #define LGFX_ESP32_2432S028
  #include <LGFX_AUTODETECT.hpp>

#else

  #include <LovyanGFX.hpp>
  class LGFX : public lgfx::LGFX_Device {
   #if DISPLAY_DRIVER == 1
    lgfx::Panel_ILI9341 _panel;
   #else
    lgfx::Panel_ST7789  _panel;
   #endif
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_XPT2046 _touch;

   public:
    LGFX() {
      {
        auto cfg = _bus.config();
        cfg.spi_host    = HSPI_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = 40000000;
        cfg.freq_read   = 16000000;
        cfg.spi_3wire   = false;
        cfg.use_lock    = true;
        cfg.dma_channel = 0;
        cfg.pin_sclk    = 14;
        cfg.pin_mosi    = 13;
        cfg.pin_miso    = 12;
        cfg.pin_dc      = 2;
        _bus.config(cfg);
        _panel.setBus(&_bus);
      }
      {
        auto cfg = _panel.config();
        cfg.pin_cs       = 15;
        cfg.pin_rst      = -1;
        cfg.pin_busy     = -1;
        cfg.panel_width  = 240;
        cfg.panel_height = 320;
        cfg.bus_shared   = false;
       #if DISPLAY_DRIVER == 1
        cfg.offset_rotation = 2;
       #endif
        _panel.config(cfg);
      }
      {
        auto cfg = _light.config();
        cfg.pin_bl      = 21;
        cfg.freq        = 12000;
        cfg.pwm_channel = 7;
        cfg.invert      = false;
        _light.config(cfg);
        _panel.setLight(&_light);
      }
      {
        auto cfg = _touch.config();
        cfg.x_min      = 300;
        cfg.x_max      = 3900;
        cfg.y_min      = 3700;
        cfg.y_max      = 200;
        cfg.pin_int    = -1;
        cfg.bus_shared = false;
        cfg.spi_host   = -1;      // the touch chip is on its own pins, driven in software
        cfg.pin_sclk   = 25;
        cfg.pin_mosi   = 32;
        cfg.pin_miso   = 39;
        cfg.pin_cs     = 33;
       #if DISPLAY_DRIVER == 2
        cfg.offset_rotation = 2;
       #endif
        _touch.config(cfg);
        _panel.setTouch(&_touch);
      }
      setPanel(&_panel);
    }
  };

#endif

#include <Preferences.h>
#include "protocol.h"
#include "ui.h"

// ---------------------------------------------------------------- STATE
static LGFX         lcd;
static UiState      ui;
static uint8_t*     artBuf = nullptr;
static FrameParser* parser = nullptr;

static View     view        = VIEW_NOLINK;
static bool     hasTrack    = false;
static uint32_t lastFrameMs = 0;       // last valid message from the computer
static uint32_t lastByteMs  = 0;

static uint32_t syncPosMs   = 0;       // position the computer last reported...
static uint32_t syncAtMs    = 0;       // ...and when it reported it
static uint32_t seekHoldUntil = 0;     // ignore reported position until then (after a scrub)
static uint32_t playHoldUntil = 0;     // ignore reported play/pause until then (after a tap)

static bool     scrubbing   = false;
static uint32_t scrubMs     = 0;
static bool     barDirty    = true;

static const uint32_t LINK_TIMEOUT_MS = 5000;

static bool before(uint32_t now, uint32_t deadline) { return (int32_t)(now - deadline) < 0; }

static uint32_t currentPosMs(uint32_t now) {
  if (scrubbing) return scrubMs;
  uint32_t p = syncPosMs;
  if (ui.playing) p += (now - syncAtMs);
  if (ui.durMs && p > ui.durMs) p = ui.durMs;
  return p;
}

static void setView(View v) {
  view = v;
  scrubbing = false;
  ui.pressed = ZONE_NONE;
  lcd.setBrightness(v == VIEW_NOLINK ? 70 : 255);
  if (v == VIEW_NOLINK)       drawNoLink();
  else if (v == VIEW_STANDBY) drawStandby();
  else {
    ui.posMs = currentPosMs(millis());
    drawPlayingScreen(ui);
    barDirty = false;
  }
}

// ---------------------------------------------------------------- MESSAGES FROM THE COMPUTER
static void copyField(char* dst, size_t n, const char*& p) {
  size_t i = 0;
  while (*p && *p != '\n') { if (i + 1 < n) dst[i++] = *p; ++p; }
  dst[i] = 0;
  if (*p == '\n') ++p;
}

static void handleFrame() {
  uint32_t now = millis();
  uint8_t type = parser->type();

  if (!parser->ok()) {
    if (type == 'A') {                       // damaged picture: drop it and ask again
      ui.artValid = false;
      if (view == VIEW_PLAYING) drawArt(ui);
      Serial.print("DT:NAK:A\n");
    }
    return;
  }
  lastFrameMs = now;

  switch (type) {
    case 'H':
      Serial.print("DT:READY:1\n");
      return;

    case 'M': {
      const char* p = (const char*)parser->payload();
      copyField(ui.title,  sizeof ui.title,  p);
      copyField(ui.artist, sizeof ui.artist, p);
      copyField(ui.album,  sizeof ui.album,  p);
      ui.artValid = false;
      hasTrack = true;
      syncPosMs = 0; syncAtMs = now; seekHoldUntil = 0;
      scrubbing = false;
      if (view != VIEW_PLAYING) setView(VIEW_PLAYING);
      else { drawArt(ui); drawTrackText(ui); barDirty = true; }
      return;
    }

    case 'S': {
      if (parser->length() < 9) return;
      const uint8_t* p = parser->payload();
      uint8_t flags = p[0];
      bool playing = flags & STATE_PLAYING;
      bool wasPlaying = ui.playing;

      if (!before(now, seekHoldUntil) && !scrubbing) {
        syncPosMs = readU32(p + 1);
        syncAtMs  = now;
      }
      if (!before(now, playHoldUntil)) {
        if (playing != ui.playing && before(now, seekHoldUntil)) {
          syncPosMs = currentPosMs(now); syncAtMs = now;   // keep the gauge where it is
        }
        ui.playing = playing;
      }
      ui.hasPos  = flags & STATE_HAS_POS;
      ui.canSeek = flags & STATE_CAN_SEEK;
      ui.durMs   = readU32(p + 5);
      if (!ui.canSeek) scrubbing = false;

      if (!hasTrack) return;                  // wait for track info before showing anything
      if (view != VIEW_PLAYING) { setView(VIEW_PLAYING); return; }
      if (ui.playing != wasPlaying) { drawStatusLabel(ui); drawButton(ZONE_PLAY, ui); }
      barDirty = true;
      return;
    }

    case 'A':
      ui.artValid = true;
      if (view == VIEW_PLAYING) drawArt(ui);
      return;

    case 'X':
      hasTrack = false;
      ui.artValid = false;
      ui.title[0] = ui.artist[0] = ui.album[0] = 0;
      if (view != VIEW_STANDBY) setView(VIEW_STANDBY);
      return;
  }
}

static void readSerial() {
  static uint8_t chunk[512];
  uint32_t now = millis();
  int avail;
  while ((avail = Serial.available()) > 0) {
    int n = Serial.readBytes(chunk, avail > (int)sizeof chunk ? sizeof chunk : avail);
    lastByteMs = now;
    for (int i = 0; i < n; ++i)
      if (parser->feed(chunk[i])) handleFrame();
  }
  if (parser->midFrame() && now - lastByteMs > 1000) parser->reset();   // message cut off
}

// ---------------------------------------------------------------- TOUCH
static void onPress(int x, int y) {
  if (view != VIEW_PLAYING) return;
  uint32_t now = millis();
  Zone z = zoneAt(x, y);

  if (z == ZONE_BAR) {
    if (ui.canSeek && ui.hasPos && ui.durMs > 0) {
      scrubbing = true;
      scrubMs = barMsFromX(x, ui.durMs);
      barDirty = true;
    }
    return;
  }
  if (z == ZONE_NONE) return;

  ui.pressed = z;
  if (z == ZONE_PLAY) {
    syncPosMs = currentPosMs(now); syncAtMs = now;
    ui.playing = !ui.playing;                 // show the change right away
    playHoldUntil = now + 1500;
    drawStatusLabel(ui);
    Serial.print("DT:CMD:PLAY\n");
  } else if (z == ZONE_NEXT) {
    Serial.print("DT:CMD:NEXT\n");
  } else {
    Serial.print("DT:CMD:PREV\n");
  }
  drawButton(z, ui);
}

static void onDrag(int x, int y) {
  (void)y;
  if (scrubbing) {
    uint32_t ms = barMsFromX(x, ui.durMs);
    if (ms != scrubMs) { scrubMs = ms; barDirty = true; }
  }
}

static void onRelease() {
  uint32_t now = millis();
  if (scrubbing) {
    scrubbing = false;
    syncPosMs = scrubMs; syncAtMs = now;
    seekHoldUntil = now + 2000;
    Serial.printf("DT:CMD:SEEK:%lu\n", (unsigned long)scrubMs);
    barDirty = true;
  }
  if (ui.pressed != ZONE_NONE) {
    int z = ui.pressed;
    ui.pressed = ZONE_NONE;
    if (view == VIEW_PLAYING) drawButton(z, ui);
  }
}

static void handleTouch() {
  static uint32_t lastPoll = 0;
  static uint8_t onCount = 0, offCount = 0;
  static bool down = false;
  uint32_t now = millis();
  if (now - lastPoll < 15) return;
  lastPoll = now;

  int32_t x = 0, y = 0;
  bool touched = lcd.getTouch(&x, &y) > 0;
  if (touched) { offCount = 0; if (onCount < 250) ++onCount; }
  else         { onCount = 0;  if (offCount < 250) ++offCount; }

  if (!down && onCount >= 2)       { down = true; onPress(x, y); }
  else if (down && touched)        { onDrag(x, y); }
  else if (down && offCount >= 3)  { down = false; onRelease(); }
}

// ---------------------------------------------------------------- TOUCH CALIBRATION
static void setupTouchCalibration() {
  Preferences prefs;
  uint16_t cal[8];
  prefs.begin("deskdisplay", false);

  // Holding a finger on the screen while it starts up forces a new calibration.
  bool force = false;
  lgfx::touch_point_t tp;
  int held = 0;
  for (int i = 0; i < 40; ++i) {
    if (lcd.getTouchRaw(&tp)) ++held; else held = 0;
    if (held >= 25) { force = true; break; }
    delay(20);
  }

  if (!force && prefs.getBytes("cal", cal, sizeof cal) == sizeof cal) {
    lcd.setTouchCalibrate(cal);
  } else {
    drawMessageScreen("TOUCH SETUP", "Lift your finger, then press", "the tip of each corner arrow.");
    while (lcd.getTouchRaw(&tp)) delay(20);
    delay(300);
    lcd.calibrateTouch(cal, COL_AMBER, COL_BG, 18);
    prefs.putBytes("cal", cal, sizeof cal);
  }
  prefs.end();
}

// ---------------------------------------------------------------- SETUP / LOOP
void setup() {
  // The board's RGB light is on when these pins are low. Turn it off.
  pinMode(4, OUTPUT);  digitalWrite(4, HIGH);
  pinMode(16, OUTPUT); digitalWrite(16, HIGH);
  pinMode(17, OUTPUT); digitalWrite(17, HIGH);

  Serial.setRxBufferSize(16384);
  Serial.begin(SERIAL_BAUD);

  artBuf = (uint8_t*)malloc(ART_BYTES);
  parser = new FrameParser(artBuf);
  ui.art = artBuf;

  if (!lcd.init()) {
    // No screen found. Blink the red light and tell the computer.
    for (;;) {
      Serial.print("DT:ERR:NODISPLAY\n");
      digitalWrite(4, LOW);  delay(300);
      digitalWrite(4, HIGH); delay(700);
    }
  }
  lcd.setRotation(SCREEN_ROTATION);
  lcd.setBrightness(255);
  uiBegin(&lcd);

  drawMessageScreen("DESK DISPLAY", "Starting up.", "Hold the screen to redo touch setup.");
  setupTouchCalibration();

  lastFrameMs = millis() - LINK_TIMEOUT_MS;
  setView(VIEW_NOLINK);
  Serial.print("\nDT:READY:1\n");
}

void loop() {
  readSerial();
  handleTouch();

  uint32_t now = millis();

  if (view != VIEW_NOLINK && now - lastFrameMs > LINK_TIMEOUT_MS) setView(VIEW_NOLINK);

  if (view == VIEW_PLAYING) {
    static uint32_t lastBarMs = 0;
    static uint32_t shownSec = 0xFFFFFFFF;
    static int shownPx = -1;
    uint32_t pos = currentPosMs(now);
    int px = ui.durMs ? (int)((uint64_t)BAR_W * pos / ui.durMs) : 0;
    bool changed = (pos / 1000 != shownSec) || (px != shownPx);
    if (barDirty || (changed && now - lastBarMs >= (scrubbing ? 30u : 200u))) {
      ui.posMs = pos;
      drawBar(ui);
      barDirty = false;
      lastBarMs = now;
      shownSec = pos / 1000;
      shownPx = px;
    }
  }
}
