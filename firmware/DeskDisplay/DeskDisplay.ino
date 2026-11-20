// DeskDisplay - a now-playing screen for the ESP32 "Cheap Yellow Display" (ESP32-2432S028R).
//
// The board shows whatever the companion app on the computer sends over the USB cable.
// Swipe down anywhere to open the menu, then tap an app:
//   MUSIC      album art, title, artist, a scrub gauge and prev / play-pause / next buttons
//   CLOCK      time and date
//   WEATHER    current conditions and the next days
//   REMINDERS  a checklist from reminders.txt on the computer, with timed alerts
//   CALENDAR   the next 7 days from a Notion database
//   SYSTEM     the computer's CPU, memory, disk and temperatures, plus this board's own
//   SETTINGS   options for each app, saved on the board. "Look" switches the whole display
//              between Minimal (amber) and Full color (a color per app, an animated sky in
//              Weather, album colors in Music)
// Touches are sent back to the computer, which controls the music app and the reminders file.
//
// Arduino IDE setup:
//   Board:   "ESP32 Dev Module"      (install "esp32 by Espressif Systems" in Boards Manager)
//   Library: "LovyanGFX"             (install from Library Manager)
//
// Look and layout live in ui.h (music, shared pieces), apps.h (menu and the other apps) and
// scene.h (the animated weather sky).
// The USB message format lives in protocol.h.

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
#include <math.h>
#include "protocol.h"
#include "ui.h"
#include "apps.h"

// ---------------------------------------------------------------- STATE
static LGFX         lcd;
static UiState      ui;
static uint8_t*     artBuf = nullptr;
static FrameParser* parser = nullptr;

static App      app         = APP_MUSIC;   // which app is showing
static bool     menuOpen    = false;
static bool     alertOpen   = false;       // a reminder alert covers everything until tapped

static bool     linkUp      = false;       // the computer is talking to us
static bool     hasTrack    = false;
static uint32_t lastFrameMs = 0;           // last valid message from the computer
static uint32_t lastByteMs  = 0;

static uint32_t syncPosMs   = 0;           // position the computer last reported...
static uint32_t syncAtMs    = 0;           // ...and when it reported it
static uint32_t seekHoldUntil = 0;         // ignore reported position until then (after a scrub)
static uint32_t playHoldUntil = 0;         // ignore reported play/pause until then (after a tap)
static bool     scrubbing   = false;
static uint32_t scrubMs     = 0;
static bool     barDirty    = true;

static ClockState   clockState;
static uint32_t     clockSyncEpoch = 0, clockSyncAtMs = 0, clockShownEpoch = 0;
static Weather      weather;
static ReminderList reminders;
static int          remPage = 0;
static bool         weatherDetails = false;   // Full color weather: the details are showing over the sky
static Alert        alert;
static EventList    events;
static int          evtPage = 0;
static Stats        stats;
static Settings     settings;
static int          setPage = 0;

static bool         artView = false;          // music: the full-screen cover is showing instead of the controls
static VizState     viz;                      // music: the visualizer bars
static uint32_t     lastMetaMs = 0;           // when the current track was announced
static uint32_t     lastTouchMs = 0;          // when the screen was last touched
static bool         saverOn = false;          // the screensaver is showing
static bool         saverFloat = false;       // ...as the floating cover (otherwise the sky)
static bool         saverRedraw = false;
static SaverState   saver;
static FloatState   floater;

static const uint32_t LINK_TIMEOUT_MS = 5000;

// How long the screen is left alone before the music controls turn into the full-screen cover,
// and how long one "minute" of the screensaver setting is. (The test harness shortens both.)
#ifndef ART_IDLE_MS
#define ART_IDLE_MS 12000
#endif
#ifndef SAVER_MINUTE_MS
#define SAVER_MINUTE_MS 60000
#endif

static bool before(uint32_t now, uint32_t deadline) { return (int32_t)(now - deadline) < 0; }

static View musicView()           { return !linkUp ? VIEW_NOLINK : (hasTrack ? VIEW_PLAYING : VIEW_STANDBY); }
static bool appVisible(App a)     { return !menuOpen && !alertOpen && !saverOn && app == a; }
static bool playingVisible()      { return appVisible(APP_MUSIC) && musicView() == VIEW_PLAYING; }
static bool controlsVisible()     { return playingVisible() && !artView; }   // the cover, text, gauge and buttons
static bool artViewVisible()      { return playingVisible() && artView; }
static uint32_t clockNow(uint32_t now) { return clockSyncEpoch + (now - clockSyncAtMs) / 1000; }

// The clock, set to this moment.
static const ClockState& clockAt(uint32_t now) {
  if (clockState.known) clockState.epoch = clockNow(now);
  return clockState;
}

static uint32_t currentPosMs(uint32_t now) {
  if (scrubbing) return scrubMs;
  uint32_t p = syncPosMs;
  if (ui.playing) p += (now - syncAtMs);
  if (ui.durMs && p > ui.durMs) p = ui.durMs;
  return p;
}

static void applyBrightness() {
  static const uint8_t LEVELS[4] = { 64, 128, 191, 255 };
  int b = LEVELS[settings.brightness & 3];
  if (appVisible(APP_MUSIC) && !linkUp) b = b * 30 / 100;     // dim the NO SIGNAL screen
  lcd.setBrightness(b);
}

// The light on the back of the board. It is lit when its pins are low.
static void setLight(bool on) {
  static int last = -1;
  if ((int)on == last) return;
  last = on;
  digitalWrite(4, on ? LOW : HIGH);     // red
  digitalWrite(16, on ? LOW : HIGH);    // green; red + green together make yellow
}

// The colors an app's screen should use. In the Minimal look that is always the amber theme.
static Palette paletteFor(App a) {
  if (!settings.fullColor) return SYSTEM_PALETTE;
  switch (a) {
    case APP_MUSIC:   return musicPal;
    case APP_CLOCK:   return clockState.known ? clockPalette(clockNow(millis())) : appPalette(a);
    case APP_WEATHER: {
      if (!weather.valid) return appPalette(a);
      float hue, sat;
      sceneHue(weather.icon, skyPhase(weather, clockAt(millis())), hue, sat);
      return paletteFromHS(hue, sat);
    }
    default:          return appPalette(a);
  }
}

// Picks the music screen's colors: from the album cover in the Full color look (if album
// colors are on), otherwise the amber theme. Returns true if the colors changed.
static bool refreshMusicPalette() {
  artBackdropBlurred = settings.fullColor;
  Palette p = (settings.fullColor && settings.albumColors && ui.artValid) ? paletteFromArt(artBuf) : SYSTEM_PALETTE;
  if (p == musicPal) return false;
  musicPal = p;
  return true;
}

// Tells the computer the settings it needs: clock format, weather unit, which alerts to send.
// Copies the settings the drawing code reads into its own variables.
static void applyLookSettings() {
  fullColorMode = settings.fullColor;
  artBackdropBlurred = settings.fullColor;
  sunGlowOn = settings.sunGlow;
  clockSkyOn = settings.clockSky;
  artvShift = settings.visualizer ? 0 : 20;
}

static void sendSettings() {
  Serial.printf("DT:SET:H24=%d,WC=%d,RA=%d,CA=%d,VZ=%d\n", settings.h24, settings.weatherC, settings.reminderAlerts,
                settings.eventAlerts, settings.visualizer);
}

static void saveSettings() {
  Preferences prefs;
  prefs.begin("deskdisplay", false);
  prefs.putBytes("set", &settings, sizeof settings);
  prefs.end();
}

static void updateBoardStats(uint32_t now) {
  float t = temperatureRead();
  // Many of these chips have no working sensor and always report 53.3 C.
  stats.chipTemp = (isnan(t) || fabsf(t - 53.33f) < 0.2f) ? STAT_UNKNOWN : (int)lroundf(t);
  stats.uptimeSec = now / 1000;
  stats.freeKb = ESP.getFreeHeap() / 1024;
  stats.linkUp = linkUp;
  if (!linkUp) stats.pcValid = false;
}

// Redraws the whole screen for whatever should be showing right now.
static void showCurrent() {
  uint32_t now = millis();
  scrubbing = false;
  ui.pressed = ZONE_NONE;
  applyBrightness();

  if (alertOpen) { saverOn = false; pal = SYSTEM_PALETTE; drawAlert(alert); return; }
  if (saverOn)   { saverRedraw = true; return; }      // the screensaver paints itself, in loop()
  if (menuOpen)  { pal = settings.fullColor ? menuPalette() : SYSTEM_PALETTE; drawMenu(app); return; }
  pal = paletteFor(app);

  switch (app) {
    case APP_CLOCK:
      clockState.epoch = clockShownEpoch = clockNow(now);
      if (clockImmersive(clockState)) drawClockSky(clockState, weather, false);
      else drawClockScreen(clockState);
      break;
    case APP_WEATHER:
      if (weatherImmersive(weather) && weatherDetails) drawWeatherDetails(weather, clockAt(now));
      else drawWeatherScreen(weather, clockAt(now));
      break;
    case APP_REMINDERS:
      if (remPage >= reminderPages(reminders)) remPage = 0;
      drawRemindersScreen(reminders, remPage);
      break;
    case APP_CALENDAR:
      if (evtPage >= eventPages(events)) evtPage = 0;
      drawCalendarScreen(events, evtPage);
      break;
    case APP_SYSTEM:
      updateBoardStats(now);
      drawSystemScreen(stats, settings);
      break;
    case APP_SETTINGS:
      drawSettingsScreen(settings, setPage);
      break;
    default:
      if (musicView() == VIEW_NOLINK)       drawNoLink();
      else if (musicView() == VIEW_STANDBY) drawStandby();
      else if (artView) {
        drawArtView(ui);
        vizReset(viz);
      } else {
        ui.posMs = currentPosMs(now);
        drawPlayingScreen(ui);
        barDirty = false;
      }
  }
}

static void rememberApp() {
  Preferences prefs;
  prefs.begin("deskdisplay", false);
  prefs.putUChar("app", (uint8_t)app);
  prefs.end();
}

// ---------------------------------------------------------------- MESSAGES FROM THE COMPUTER
static void handleFrame() {
  uint32_t now = millis();
  uint8_t type = parser->type();

  if (!parser->ok()) {
    if (type == 'A') {                       // damaged picture: drop it and ask again
      ui.artValid = false;
      if (controlsVisible()) drawArt(ui);
      Serial.print("DT:NAK:A\n");
    }
    return;
  }

  View viewBefore = musicView();
  bool partial = controlsVisible();          // the music controls are up: update just the changed part
  bool coverUp = artViewVisible();           // the full-screen cover is up
  lastFrameMs = now;
  linkUp = true;
  const char* text = (const char*)parser->payload();

  switch (type) {
    case 'H':
      Serial.print("DT:READY:1\n");
      sendSettings();
      break;

    case 'M':
      takeLine(ui.title,  sizeof ui.title,  text);
      takeLine(ui.artist, sizeof ui.artist, text);
      takeLine(ui.album,  sizeof ui.album,  text);
      ui.artValid = false;
      hasTrack = true;
      syncPosMs = 0; syncAtMs = now; seekHoldUntil = 0;
      scrubbing = false;
      lastMetaMs = now;
      memset(artBlur, 0x1C, sizeof artBlur);   // a plain dark backdrop until the new cover arrives
      if (partial) { drawArt(ui); drawTrackText(ui); barDirty = true; }
      if (coverUp) showCurrent();
      if (saverOn && saverFloat) saverRedraw = true;
      break;

    case 'S': {
      if (parser->length() < 9) break;
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
      if (partial) {
        if (ui.playing != wasPlaying) { drawStatusLabel(ui); drawButton(ZONE_PLAY, ui); }
        barDirty = true;
      }
      break;
    }

    case 'A':
      ui.artValid = true;
      buildArtBlur(artBuf);
      if (coverUp) { refreshMusicPalette(); showCurrent(); }
      else if (refreshMusicPalette()) { if (partial) showCurrent(); }   // new album colors: repaint it all
      else if (partial) drawArt(ui);
      if (saverOn && saverFloat) saverRedraw = true;
      break;

    case 'V':                                  // sound levels for the visualizer
      if (parser->length() >= (uint32_t)VIZ_BARS) {
        memcpy(viz.target, parser->payload(), VIZ_BARS);
        viz.lastDataMs = now;
      }
      break;

    case 'X':
      artView = false;
      hasTrack = false;
      ui.artValid = false;
      ui.title[0] = ui.artist[0] = ui.album[0] = 0;
      musicPal = SYSTEM_PALETTE;
      break;

    case 'T': {
      if (parser->length() < 5) break;
      const uint8_t* p = parser->payload();
      bool wasKnown = clockState.known;
      clockSyncEpoch = readU32(p);
      clockSyncAtMs  = now;
      clockState.known = true;
      if (appVisible(APP_CLOCK) && !wasKnown) showCurrent();
      break;
    }

    case 'W':
      parseWeather(text, weather);
      if (appVisible(APP_WEATHER)) showCurrent();
      break;

    case 'R':
      parseReminders(text, reminders);
      if (appVisible(APP_REMINDERS)) showCurrent();
      break;

    case 'E':
      parseEvents(text, events);
      if (appVisible(APP_CALENDAR)) showCurrent();
      break;

    case 'Y':
      parseStats(text, stats);
      if (appVisible(APP_SYSTEM)) { updateBoardStats(now); drawSystemValues(stats, settings); }
      break;

    case 'N':
      parseAlert(text, alert);
      alertOpen = true;
      showCurrent();
      break;
  }

  if (appVisible(APP_MUSIC) && musicView() != viewBefore) showCurrent();
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
// Buttons and the scrub gauge react the moment they are touched. Everywhere else a touch is a
// "gesture": it is judged when the finger lifts, as a tap, a swipe down (open the menu) or a
// swipe up (close the menu).
static bool gesture = false;
static int  startX = 0, startY = 0;
static int  trailX[3] = { 0, 0, 0 }, trailY[3] = { 0, 0, 0 };
static int  trailLen = 0;
static int  pressedTile = -1;
static bool swallowTouch = false;    // this touch only woke the screensaver
static uint32_t openButtonLitMs = 0; // when Open Apple Music was tapped (0 = not lit)

static const int SWIPE_MIN = 45;     // pixels a finger must travel to count as a swipe
static const int TAP_MAX   = 18;     // pixels a finger may wander and still count as a tap

// A row in the Settings app was tapped: change it, save it, and make it take effect.
static void changeSetting(int i) {
  settingAdvance(settings, i);
  saveSettings();
  drawSettingsRow(settings, setPage, i);
  switch (i) {
    case SET_LOOK:                            // the look changed: repaint in the new colors
      applyLookSettings();
      weatherDetails = false;
      refreshMusicPalette();
      showCurrent();
      break;
    case SET_ALBUM_COLORS: refreshMusicPalette(); break;
    case SET_TIME: clockState.h24 = settings.h24; sendSettings(); break;
    case SET_WEATHER_UNIT: case SET_REMINDER_ALERTS: case SET_EVENT_ALERTS: sendSettings(); break;
    case SET_BRIGHTNESS: applyBrightness(); break;
    case SET_FULL_ART: if (!settings.cover) artView = false; break;
    case SET_VISUALIZER: applyLookSettings(); sendSettings(); break;   // the companion stops or starts sending sound levels
    case SET_CLOCK_SKY: case SET_SUN_GLOW: applyLookSettings(); break;
  }
}

static void onTap(int x, int y) {
  if (menuOpen) {
    int t = menuTileAt(x, y);
    if (t >= 0) {
      app = (App)t;
      weatherDetails = false;
      menuOpen = false;
      rememberApp();
      showCurrent();
    }
    return;
  }
  if (app == APP_MUSIC && musicView() == VIEW_PLAYING) {
    // Tapping the cover makes it fill the screen; tapping the full-screen cover brings the controls back.
    if (artView) { artView = false; showCurrent(); }
    else if (settings.cover && x < ART_X + ART_SIZE + 4 && y < BAR_AREA_Y - 6) { artView = true; showCurrent(); }
  } else if (app == APP_MUSIC && musicView() == VIEW_STANDBY && openButtonAt(x, y)) {
    Serial.print("DT:CMD:OPENAPP\n");          // the computer opens Apple Music
    drawOpenButton(true);
    openButtonLitMs = millis();
  } else if (app == APP_REMINDERS && reminders.valid) {
    int i = reminderAt(reminders, remPage, y);
    if (i == -2) {
      int pages = reminderPages(reminders);
      if (pages > 1) { remPage = (remPage + 1) % pages; showCurrent(); }
    } else if (i >= 0) {
      Reminder& it = reminders.items[i];
      it.done = !it.done;                     // show the change right away
      drawReminderRow(reminders, remPage, i);
      Serial.printf("DT:CMD:REM:%d\n", i);
    }
  } else if (app == APP_CALENDAR && events.valid) {
    int pages = eventPages(events);
    if (y >= REM_FOOT_Y && pages > 1) { evtPage = (evtPage + 1) % pages; showCurrent(); }
  } else if (app == APP_SETTINGS) {
    int i = settingAt(setPage, y);
    if (i == -2) { setPage = (setPage + 1) % settingsPages(); showCurrent(); }
    else if (i >= 0) changeSetting(i);
  }
}

static void onPress(int x, int y) {
  uint32_t now = millis();
  startX = x; startY = y;
  trailX[0] = x; trailY[0] = y; trailLen = 1;
  pressedTile = -1;
  gesture = true;
  if (saverOn) {                              // the first touch only wakes the screen
    saverOn = false;
    swallowTouch = true;
    showCurrent();
    return;
  }
  if (alertOpen) return;

  if (menuOpen) {
    pressedTile = menuTileAt(x, y);
    if (pressedTile >= 0) drawMenuTile(pressedTile, pressedTile == app, true);
    return;
  }
  if (!controlsVisible()) return;

  Zone z = zoneAt(x, y);
  if (z == ZONE_NONE) return;
  gesture = false;                            // a button or the gauge: act now

  if (z == ZONE_BAR) {
    if (ui.canSeek && ui.hasPos && ui.durMs > 0) {
      scrubbing = true;
      scrubMs = barMsFromX(x, ui.durMs);
      barDirty = true;
    }
    return;
  }
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
  if (swallowTouch) return;
  if (trailLen < 3) { trailX[trailLen] = x; trailY[trailLen] = y; ++trailLen; }
  else { trailX[0] = trailX[1]; trailY[0] = trailY[1]; trailX[1] = trailX[2]; trailY[1] = trailY[2]; trailX[2] = x; trailY[2] = y; }

  if (scrubbing) {
    uint32_t ms = barMsFromX(x, ui.durMs);
    if (ms != scrubMs) { scrubMs = ms; barDirty = true; }
  }
  if (pressedTile >= 0 && (abs(x - startX) > TAP_MAX || abs(y - startY) > TAP_MAX)) {
    if (menuOpen) drawMenuTile(pressedTile, pressedTile == app, false);
    pressedTile = -1;
  }
}

static void onRelease() {
  uint32_t now = millis();
  if (swallowTouch) { swallowTouch = false; gesture = false; return; }
  if (!gesture) {
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
      if (controlsVisible()) drawButton(z, ui);
    }
    return;
  }
  gesture = false;

  if (alertOpen) {                            // any touch dismisses an alert
    alertOpen = false;
    showCurrent();
    return;
  }

  // The very last reading as a finger lifts is often off, so use the one before it.
  int k = trailLen >= 3 ? 1 : trailLen - 1;
  int dx = trailX[k] - startX, dy = trailY[k] - startY;

  // In the full-screen weather sky, swiping up brings up the details and swiping down puts
  // them away again. Everywhere else, down opens the menu and up closes it.
  bool sky = !menuOpen && app == APP_WEATHER && weatherImmersive(weather);

  if (dy >= SWIPE_MIN && abs(dx) < 120) {
    if (sky && weatherDetails) { weatherDetails = false; showCurrent(); }
    else if (!menuOpen) { menuOpen = true; showCurrent(); }
    else if (pressedTile >= 0) drawMenuTile(pressedTile, pressedTile == app, false);
  } else if (dy <= -SWIPE_MIN && abs(dx) < 120) {
    if (menuOpen) { menuOpen = false; showCurrent(); }
    else if (sky && !weatherDetails) { weatherDetails = true; showCurrent(); }
  } else if (abs(dx) <= TAP_MAX && abs(dy) <= TAP_MAX) {
    onTap(startX, startY);
  } else if (menuOpen && pressedTile >= 0) {
    drawMenuTile(pressedTile, pressedTile == app, false);
  }
  pressedTile = -1;
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
  if (touched) { offCount = 0; if (onCount < 250) ++onCount; lastTouchMs = now; }
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
  uint8_t saved = prefs.getUChar("app", 0);
  app = (App)(saved < APP_COUNT ? saved : 0);
  Settings stored;
  if (prefs.getBytes("set", &stored, sizeof stored) == sizeof stored && stored.version == settings.version) settings = stored;
  settings.brightness &= 3;
  settings.saver &= 3;
  if (settings.cover > 2) settings.cover = 2;
  applyLookSettings();
  clockState.h24 = settings.h24;
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
  sceneBegin(&lcd);

  drawMessageScreen("DESK DISPLAY", "Starting up.", "Hold the screen to redo touch setup.");
  setupTouchCalibration();

  showCurrent();
  Serial.print("\nDT:READY:1\n");
  sendSettings();
}

// ---- screensaver
// The cover floats while music plays (it keeps floating for a few seconds while the next
// track's cover is on its way); otherwise the sky with a drifting clock.
static bool saverWantsFloat(uint32_t now) {
  return hasTrack && ui.playing && (ui.artValid || now - lastMetaMs < 8000);
}

static void runSaver(uint32_t now) {
  static uint32_t lastMs = 0;
  bool wantFloat = saverWantsFloat(now);
  if (wantFloat != saverFloat) { saverFloat = wantFloat; saverRedraw = true; }
  if (saverRedraw) {
    saverRedraw = false;
    lastMs = now;
    if (saverFloat) drawFloatStart(ui, floater);
    else drawSaverSky(clockAt(now), weather, saver, false);
    return;
  }
  if (now - lastMs < (saverFloat ? 60u : SCENE_FRAME_MS)) return;
  lastMs = now;
  if (saverFloat) drawFloatStep(ui, floater);
  else drawSaverSky(clockAt(now), weather, saver, true);
}

void loop() {
  readSerial();
  handleTouch();

  uint32_t now = millis();

  if (linkUp && now - lastFrameMs > LINK_TIMEOUT_MS) {
    linkUp = false;
    if (saverOn) { saverOn = false; showCurrent(); }
    else if (appVisible(APP_MUSIC)) showCurrent();
  }

  // Blink the light on the back while a reminder or event alert is on screen.
  setLight(alertOpen && settings.alertLight && (now / 350) % 2 == 0);

  // Left alone for long enough: start the screensaver. While music plays, show the music
  // (the full-screen cover with its bars, if that setting allows) instead of a screensaver.
  if (!saverOn && !alertOpen && linkUp && (settings.saver & 3) &&
      now - lastTouchMs >= (uint32_t)SAVER_MINUTES[settings.saver & 3] * SAVER_MINUTE_MS) {
    if (hasTrack && ui.playing) {
      if (!playingVisible() || (!artView && settings.cover)) {
        app = APP_MUSIC;
        menuOpen = false;
        weatherDetails = false;
        scrubbing = false;
        ui.pressed = ZONE_NONE;
        if (settings.cover) artView = true;
        showCurrent();
      }
    } else {
    saverOn = true;
    saverFloat = saverWantsFloat(now);
    saverRedraw = true;
    scrubbing = false;
    ui.pressed = ZONE_NONE;
    }
  }
  if (saverOn) { runSaver(now); return; }

  if (openButtonLitMs && now - openButtonLitMs > 4000) {      // nothing started: back to the plain button
    openButtonLitMs = 0;
    if (appVisible(APP_MUSIC) && musicView() == VIEW_STANDBY) drawOpenButton(false);
  }

  if (appVisible(APP_SYSTEM)) {
    static uint32_t lastStatsMs = 0;
    if (now - lastStatsMs >= 1000) {
      lastStatsMs = now;
      updateBoardStats(now);
      drawSystemValues(stats, settings);
    }
  }

  // Music left alone while it plays: the controls give way to the full-screen cover.
  if (controlsVisible() && settings.cover == 2 && ui.playing && ui.artValid && !scrubbing &&
      ui.pressed == ZONE_NONE && now - lastTouchMs >= ART_IDLE_MS && now - lastMetaMs >= 1500) {
    artView = true;
    showCurrent();
  }

  if (artViewVisible() && settings.visualizer) {   // the visualizer bars
    static uint32_t lastVizMs = 0;
    if (now - lastVizMs >= 30) {                 // about 33 times a second
      lastVizMs = now;
      drawVizBars(viz, ui.playing && now - viz.lastDataMs < 600);
    }
  }

  if (controlsVisible()) {
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

  // The animated sky in the Weather app (Full color look only).
  if (appVisible(APP_WEATHER) && weatherImmersive(weather) && !weatherDetails) {
    static uint32_t lastSceneMs = 0;
    if (now - lastSceneMs >= SCENE_FRAME_MS) {
      lastSceneMs = now;
      drawWeatherSky(weather, clockAt(now), true);
    }
  }

  if (appVisible(APP_CLOCK) && clockState.known) {
    uint32_t e = clockNow(now);
    if (clockImmersive(clockState)) {             // Full color: the time on the animated sky
      static uint32_t lastClockSkyMs = 0;
      if (now - lastClockSkyMs >= SCENE_FRAME_MS) {
        lastClockSkyMs = now;
        clockState.epoch = clockShownEpoch = e;
        drawClockSky(clockState, weather, true);
      }
    } else if (e != clockShownEpoch) {
      clockState.epoch = e;
      if (e / 60 != clockShownEpoch / 60) {
        if (!(paletteFor(APP_CLOCK) == pal)) { showCurrent(); return; }   // the time of day moved on: new colors
        drawClockTime(clockState);
        if (e / 86400 != clockShownEpoch / 86400) drawClockDate(clockState);
      }
      drawClockSeconds(clockState);
      clockShownEpoch = e;
    }
  }
}
