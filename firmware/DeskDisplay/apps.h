// apps.h - the menu and the Clock, Weather, Reminders, Calendar, System and Settings screens.
// Uses the colors, fonts and helpers from ui.h.
#pragma once
#include "ui.h"
#include "scene.h"
#include <time.h>
#include <stdlib.h>

enum App { APP_MUSIC = 0, APP_CLOCK, APP_WEATHER, APP_REMINDERS, APP_CALENDAR, APP_SYSTEM, APP_SETTINGS, APP_COUNT };

// ---------------------------------------------------------------- data the computer sends
struct ClockState {
  bool known = false;
  uint32_t epoch = 0;        // local time right now, seconds since 1970
  bool h24 = false;
};

struct WeatherDay { char name[8] = ""; int icon = 2; char hi[8] = ""; char lo[8] = ""; };
struct Weather {
  bool valid = false;        // false: show 'error' instead
  char error[64] = "Waiting for the computer.";
  char city[28] = "", temp[8] = "", unit[4] = "F", cond[28] = "";
  int  icon = 2;
  char hi[8] = "", lo[8] = "", feels[8] = "", wind[14] = "", hum[8] = "", updated[12] = "";
  bool night = false;
  int  sunrise = -1, sunset = -1;      // minutes after midnight, -1 = not known
  WeatherDay days[3];
};

static const int REM_MAX = 10, REM_ROWS = 5;
struct Reminder { bool done = false; char time[10] = ""; char text[66] = ""; };
struct ReminderList {
  bool valid = false;
  int count = 0;             // items in 'items'
  int open = 0;              // not done yet, in the whole file
  Reminder items[REM_MAX];
};

struct Alert { char time[10] = ""; char text[66] = ""; };

static const int EVT_MAX = 10, LIST_ROWS = 5;
struct CalEvent { char day[10] = ""; char time[10] = ""; char title[66] = ""; };
struct EventList {
  bool valid = false;        // false: show 'error' instead
  char error[64] = "Waiting for the computer.";
  int count = 0;
  CalEvent items[EVT_MAX];
};

static const int STAT_UNKNOWN = -1000;
struct Stats {
  bool pcValid = false;      // the computer has sent numbers
  int cpu = 0, mem = 0, disk = 0;                         // percent
  int cpuTemp = STAT_UNKNOWN, gpuTemp = STAT_UNKNOWN;     // degrees C
  int chipTemp = STAT_UNKNOWN;                            // this board, degrees C
  uint32_t uptimeSec = 0;
  uint32_t freeKb = 0;
  bool linkUp = false;
};

// Everything the Settings app can change. Saved on the board.
struct Settings {
  uint8_t version     = 4;
  bool fullColor      = true;    // DISPLAY: the look. false = Minimal (amber), true = Full color
  bool albumColors    = true;    // MUSIC: in Full color, take the music screen's colors from the cover
  bool h24            = false;   // CLOCK: 24-hour time (also used for times in other apps)
  bool weatherC       = true;    // WEATHER: Celsius (false = Fahrenheit)
  bool reminderAlerts = true;    // REMINDERS: full-screen alert for timed reminders
  bool eventAlerts    = true;    // CALENDAR: alert 10 minutes before an event
  bool statsF         = false;   // SYSTEM: Fahrenheit instead of Celsius
  uint8_t brightness  = 3;       // DISPLAY: 0..3 = 25, 50, 75, 100 %
  bool alertLight     = true;    // DISPLAY: blink the light on the back during an alert
  uint8_t cover       = 2;       // MUSIC: full-screen cover 0 = off, 1 = only when the cover is tapped, 2 = also by itself
  uint8_t saver       = 2;       // DISPLAY: screensaver after 0 = never, 1 = 1, 2 = 5, 3 = 15 minutes
  bool visualizer     = true;    // MUSIC: bars along the bottom of the full-screen cover
  bool clockSky       = true;    // CLOCK: in Full color, the time sits on the moving sky
  bool sunGlow        = true;    // SKIES: in Full color, orange and pink skies around sunrise and sunset
};

// ---------------------------------------------------------------- reading the text messages
static void takeField(char* dst, size_t n, const char*& p) {      // up to the next '|'
  size_t i = 0;
  while (*p && *p != '|' && *p != '\n') { if (i + 1 < n) dst[i++] = *p; ++p; }
  dst[i] = 0;
  if (*p == '|') ++p;
}
static void takeLine(char* dst, size_t n, const char*& p) {       // up to the end of the line
  size_t i = 0;
  while (*p && *p != '\n') { if (i + 1 < n) dst[i++] = *p; ++p; }
  dst[i] = 0;
  if (*p == '\n') ++p;
}

// "OK" city temp unit condition icon high low feels wind humidity updated sun, one per line,
// then up to three lines of "day|icon|high|low".   Or: "ERR" and a message line.
// The sun line is "D" (day) or "N" (night), optionally followed by "|sunrise|sunset" in minutes
// after midnight, for example "D|387|1012".
static void parseWeather(const char* p, Weather& w) {
  char tmp[16];
  takeLine(tmp, sizeof tmp, p);
  if (strcmp(tmp, "OK") != 0) {
    w.valid = false;
    takeLine(w.error, sizeof w.error, p);
    return;
  }
  w.valid = true;
  takeLine(w.city, sizeof w.city, p);
  takeLine(w.temp, sizeof w.temp, p);
  takeLine(w.unit, sizeof w.unit, p);
  takeLine(w.cond, sizeof w.cond, p);
  takeLine(tmp, sizeof tmp, p); w.icon = atoi(tmp);
  takeLine(w.hi, sizeof w.hi, p);
  takeLine(w.lo, sizeof w.lo, p);
  takeLine(w.feels, sizeof w.feels, p);
  takeLine(w.wind, sizeof w.wind, p);
  takeLine(w.hum, sizeof w.hum, p);
  takeLine(w.updated, sizeof w.updated, p);
  char sun[24];
  takeLine(sun, sizeof sun, p);
  const char* q = sun;
  takeField(tmp, sizeof tmp, q); w.night = (tmp[0] == 'N');
  w.sunrise = w.sunset = -1;
  if (*q) {
    takeField(tmp, sizeof tmp, q); int rise = atoi(tmp);
    takeField(tmp, sizeof tmp, q); int set = atoi(tmp);
    if (rise >= 0 && set > rise && set < 1440) { w.sunrise = rise; w.sunset = set; }
  }
  for (int i = 0; i < 3; ++i) {
    WeatherDay& d = w.days[i];
    takeField(d.name, sizeof d.name, p);
    takeField(tmp, sizeof tmp, p); d.icon = atoi(tmp);
    takeField(d.hi, sizeof d.hi, p);
    takeLine(d.lo, sizeof d.lo, p);
  }
}

// First line: how many are still open. Then one line per reminder: "done(0/1)|time|text".
static void parseReminders(const char* p, ReminderList& r) {
  char tmp[16];
  takeLine(tmp, sizeof tmp, p);
  r.open = atoi(tmp);
  r.count = 0;
  while (*p && r.count < REM_MAX) {
    Reminder& it = r.items[r.count];
    takeField(tmp, sizeof tmp, p); it.done = (tmp[0] == '1');
    takeField(it.time, sizeof it.time, p);
    takeLine(it.text, sizeof it.text, p);
    if (it.text[0]) ++r.count;
  }
  r.valid = true;
}

// "OK" then one line per event: "day|time|title".   Or: "ERR" and a message line.
static void parseEvents(const char* p, EventList& e) {
  char tmp[16];
  takeLine(tmp, sizeof tmp, p);
  e.count = 0;
  if (strcmp(tmp, "OK") != 0) {
    e.valid = false;
    takeLine(e.error, sizeof e.error, p);
    return;
  }
  e.valid = true;
  while (*p && e.count < EVT_MAX) {
    CalEvent& it = e.items[e.count];
    takeField(it.day, sizeof it.day, p);
    takeField(it.time, sizeof it.time, p);
    takeLine(it.title, sizeof it.title, p);
    if (it.title[0]) ++e.count;
  }
}

// Five lines: cpu %, memory %, disk %, cpu temperature, graphics temperature (C, or "-").
static void parseStats(const char* p, Stats& s) {
  char tmp[16];
  takeLine(tmp, sizeof tmp, p); s.cpu = atoi(tmp);
  takeLine(tmp, sizeof tmp, p); s.mem = atoi(tmp);
  takeLine(tmp, sizeof tmp, p); s.disk = atoi(tmp);
  takeLine(tmp, sizeof tmp, p); s.cpuTemp = (tmp[0] && tmp[0] != '-') ? atoi(tmp) : STAT_UNKNOWN;
  takeLine(tmp, sizeof tmp, p); s.gpuTemp = (tmp[0] && tmp[0] != '-') ? atoi(tmp) : STAT_UNKNOWN;
  s.pcValid = true;
}

static void parseAlert(const char* p, Alert& a) {
  takeLine(a.time, sizeof a.time, p);
  takeLine(a.text, sizeof a.text, p);
}

// ---------------------------------------------------------------- the two looks
// Minimal: every screen uses the amber THEME. Full color: each app has its own color, the
// Weather app gets an animated sky, the Clock follows the time of day, and Music follows the cover.
static bool fullColorMode = false;
static bool sunGlowOn     = true;     // copies of settings.sunGlow and settings.clockSky, for the drawing code
static bool clockSkyOn    = true;

static const float APP_HUE[APP_COUNT] = { 38, 265, 208, 140, 5, 178, 215 };
static const float APP_SAT[APP_COUNT] = { 0.85f, 0.60f, 0.75f, 0.55f, 0.70f, 0.60f, 0.15f };
static Palette appPalette(int app) { return paletteFromHS(APP_HUE[app], APP_SAT[app]); }
static Palette menuPalette()       { return paletteFromHS(220, 0.12f); }

// Dawn is warm, day is blue, dusk is purple, night is deep blue.
static Palette clockPalette(uint32_t epoch) {
  int hour = (int)((epoch / 3600) % 24);
  if (hour >= 5 && hour < 8)   return paletteFromHS(22, 0.75f);
  if (hour >= 8 && hour < 17)  return paletteFromHS(208, 0.70f);
  if (hour >= 17 && hour < 20) return paletteFromHS(300, 0.50f);
  return paletteFromHS(232, 0.60f);
}

// ---------------------------------------------------------------- the sky right now
// Sunrise and sunset come with the weather. Without them the sky assumes 6:30 and 18:30.
static const int GLOW_MINUTES = 35;        // the sky is orange and pink this long before and after each

static void sunTimes(const Weather& w, int& rise, int& set) {
  if (w.valid && w.sunrise >= 0) { rise = w.sunrise; set = w.sunset; }
  else { rise = 6 * 60 + 30; set = 18 * 60 + 30; }
}

static int skyPhase(const Weather& w, const ClockState& c) {
  bool haveSun = w.valid && w.sunrise >= 0;
  if (!c.known || (w.valid && !haveSun)) return (w.valid && w.night) ? PHASE_NIGHT : PHASE_DAY;
  int rise, set; sunTimes(w, rise, set);
  int m = (int)((c.epoch / 60) % 1440);
  if (sunGlowOn && abs(m - rise) <= GLOW_MINUTES) return PHASE_DAWN;
  if (sunGlowOn && abs(m - set) <= GLOW_MINUTES)  return PHASE_DUSK;
  return (m > rise && m < set) ? PHASE_DAY : PHASE_NIGHT;
}

// The sky for the weather as it is, with the sun in its usual corner.
static SceneInfo skyInfo(const Weather& w, const ClockState& c) {
  SceneInfo info;
  info.kind = w.valid ? w.icon : 0;
  info.phase = skyPhase(w, c);
  return info;
}

// Moves the sun (or the moon, at night) to where it is on its arc across the top of the screen:
// rising at the left, highest in the middle of the day or night, setting at the right.
static void sunOnArc(const Weather& w, const ClockState& c, SceneInfo& info) {
  int rise, set; sunTimes(w, rise, set);
  int m = c.known ? (int)((c.epoch / 60) % 1440) : 12 * 60;
  float f;
  if (info.phase != PHASE_NIGHT) {
    f = (float)(m - rise) / (float)(set - rise);
  } else {
    int since = m >= set ? m - set : m + 1440 - set;
    f = (float)since / (float)(1440 - (set - rise));
  }
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  info.sunX = 36 + (int)lroundf(f * 248);
  info.sunY = 66 - (int)lroundf(sinf(f * 3.14159265f) * 32);
  info.sunR = 18;
}

// ---------------------------------------------------------------- shared pieces
static void drawHeader(const char* title, const char* right = nullptr) {
  drawPullHandle();
  gfx->setFont(FONT_SMALL);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->fillRect(8, 12, 7, 7, pal.accent);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(title, 20, 12);
  if (right) {
    gfx->setTextDatum(lgfx::textdatum_t::top_right);
    gfx->setTextColor(pal.dim, pal.bg);
    gfx->drawString(right, SCREEN_W - 8, 12);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
  }
  gfx->drawFastHLine(8, 24, SCREEN_W - 16, pal.dim);
}

static void thickLine(int x0, int y0, int x1, int y1, uint32_t col) {
  gfx->drawLine(x0, y0, x1, y1, col);
  gfx->drawLine(x0 + 1, y0, x1 + 1, y1, col);
  gfx->drawLine(x0, y0 + 1, x1, y1 + 1, col);
}

static void drawSun(int cx, int cy, int r, uint32_t col) {
  gfx->fillCircle(cx, cy, r * 48 / 100, col);
  static const int8_t DX[8] = { 100, 71, 0, -71, -100, -71, 0, 71 };
  static const int8_t DY[8] = { 0, 71, 100, 71, 0, -71, -100, -71 };
  for (int i = 0; i < 8; ++i)
    thickLine(cx + DX[i] * r * 66 / 10000, cy + DY[i] * r * 66 / 10000,
              cx + DX[i] * r / 100,        cy + DY[i] * r / 100, col);
}

static void drawCloud(int cx, int cy, int w, uint32_t col) {
  gfx->fillCircle(cx - w * 24 / 100, cy + w * 6 / 100, w * 20 / 100, col);
  gfx->fillCircle(cx + w * 2 / 100,  cy - w * 8 / 100, w * 28 / 100, col);
  gfx->fillCircle(cx + w * 28 / 100, cy + w * 8 / 100, w * 18 / 100, col);
  gfx->fillRect(cx - w * 24 / 100, cy + w * 6 / 100, w * 52 / 100, w * 21 / 100, col);
}

// icon: 0 clear, 1 partly cloudy, 2 cloudy, 3 fog, 4 rain, 5 snow, 6 storm.  size = width in pixels.
static void drawWeatherIcon(int icon, int cx, int cy, int size, uint32_t colSun, uint32_t colCloud, uint32_t colBg) {
  int r = size / 2;
  switch (icon) {
    case 0:
      drawSun(cx, cy, r, colSun);
      break;
    case 1:
      drawSun(cx - r * 30 / 100, cy - r * 30 / 100, r * 62 / 100, colSun);
      drawCloud(cx + r * 14 / 100, cy + r * 28 / 100, size * 92 / 100, colBg);
      drawCloud(cx + r * 14 / 100, cy + r * 28 / 100, size * 80 / 100, colCloud);
      break;
    case 3:
      for (int i = 0; i < 4; ++i) {
        int w = size * (i % 2 ? 60 : 84) / 100, h = size > 40 ? 5 : 3;
        gfx->fillRect(cx - w / 2, cy - r * 60 / 100 + i * r * 40 / 100, w, h, i % 2 ? colSun : colCloud);
      }
      break;
    default: {
      bool wet = icon >= 4;
      int ccy = wet ? cy - r * 22 / 100 : cy;
      drawCloud(cx, ccy, size * 88 / 100, colCloud);
      int by = ccy + size * 30 / 100;
      if (icon == 4) {
        for (int i = -1; i <= 1; ++i)
          thickLine(cx + i * r * 42 / 100 + r * 10 / 100, by, cx + i * r * 42 / 100 - r * 10 / 100, by + r * 42 / 100, colSun);
      } else if (icon == 5) {
        for (int i = -1; i <= 1; ++i)
          gfx->fillCircle(cx + i * r * 42 / 100, by + r * 22 / 100 + (i ? 0 : r * 16 / 100), size > 40 ? 3 : 2, colSun);
      } else if (icon == 6) {
        gfx->fillTriangle(cx + r * 12 / 100, by - r * 14 / 100, cx - r * 22 / 100, by + r * 26 / 100, cx + r * 6 / 100, by + r * 26 / 100, colSun);
        gfx->fillTriangle(cx - r * 4 / 100, by + r * 16 / 100, cx + r * 24 / 100, by + r * 16 / 100, cx - r * 10 / 100, by + r * 62 / 100, colSun);
      }
    }
  }
}

// ---------------------------------------------------------------- MENU
static const int TILE_W = 98, TILE_H = 62;
static const int TILE_X[3] = { 8, 111, 214 };
static const int TILE_Y[3] = { 30, 98, 166 };
static const char* const APP_NAMES[APP_COUNT] = { "MUSIC", "CLOCK", "WEATHER", "REMINDERS", "CALENDAR", "SYSTEM", "SETTINGS" };

static int menuTileAt(int x, int y) {
  if (y < TILE_Y[0] - 3) return -1;
  int col = x < TILE_X[1] - 3 ? 0 : (x < TILE_X[2] - 3 ? 1 : 2);
  int row = y < TILE_Y[1] - 3 ? 0 : (y < TILE_Y[2] - 3 ? 1 : 2);
  int i = row * 3 + col;
  return i < APP_COUNT ? i : -1;
}

static void drawAppIcon(int i, int cx, int cy, uint32_t ink, uint32_t face) {
  switch (i) {
    case APP_MUSIC:
      gfx->fillCircle(cx - 8, cy + 8, 5, ink);
      gfx->fillCircle(cx + 8, cy + 5, 5, ink);
      gfx->fillRect(cx - 5, cy - 10, 2, 19, ink);
      gfx->fillRect(cx + 11, cy - 13, 2, 19, ink);
      gfx->fillTriangle(cx - 5, cy - 10, cx + 13, cy - 13, cx + 13, cy - 8, ink);
      gfx->fillTriangle(cx - 5, cy - 10, cx - 5, cy - 5, cx + 13, cy - 8, ink);
      break;
    case APP_CLOCK:
      gfx->fillCircle(cx, cy, 13, ink);
      gfx->fillCircle(cx, cy, 10, face);
      gfx->fillRect(cx - 1, cy - 8, 2, 9, ink);
      gfx->fillRect(cx - 1, cy - 1, 7, 2, ink);
      break;
    case APP_WEATHER:
      drawWeatherIcon(1, cx, cy, 30, ink, ink, face);
      break;
    case APP_REMINDERS:
      for (int row = 0; row < 2; ++row) {
        int ry = cy - 11 + row * 13;
        gfx->drawRect(cx - 17, ry, 9, 9, ink);
        gfx->fillRect(cx - 4, ry + 3, 21, 3, ink);
      }
      gfx->drawLine(cx - 15, cy - 7, cx - 13, cy - 5, ink);
      gfx->drawLine(cx - 13, cy - 5, cx - 10, cy - 9, ink);
      break;
    case APP_CALENDAR:
      gfx->fillRect(cx - 13, cy - 10, 26, 22, ink);
      gfx->fillRect(cx - 11, cy - 3, 22, 13, face);
      gfx->fillRect(cx - 8, cy - 13, 3, 5, ink);
      gfx->fillRect(cx + 5, cy - 13, 3, 5, ink);
      for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 3; ++c) gfx->fillRect(cx - 8 + c * 7, cy + r * 5, 3, 3, ink);
      break;
    case APP_SYSTEM:
      gfx->fillRect(cx - 9, cy - 9, 18, 18, ink);
      gfx->fillRect(cx - 5, cy - 5, 10, 10, face);
      for (int k = -1; k <= 1; ++k) {
        gfx->fillRect(cx + k * 6 - 1, cy - 13, 2, 4, ink);
        gfx->fillRect(cx + k * 6 - 1, cy + 9, 2, 4, ink);
        gfx->fillRect(cx - 13, cy + k * 6 - 1, 4, 2, ink);
        gfx->fillRect(cx + 9, cy + k * 6 - 1, 4, 2, ink);
      }
      break;
    default:
      for (int k = 0; k < 3; ++k) {
        int ly = cy - 9 + k * 9;
        gfx->fillRect(cx - 14, ly, 28, 2, ink);
        gfx->fillRect(cx - 9 + ((k * 11) % 17), ly - 3, 5, 8, ink);
      }
  }
}

static void drawMenuTile(int i, bool selected, bool pressed) {
  int x = TILE_X[i % 3], y = TILE_Y[i / 3];
  uint32_t color = fullColorMode ? appPalette(i).accent : pal.accent;    // each app's own color
  uint32_t face = pressed ? pal.cream : (selected ? color : pal.bg);
  uint32_t ink  = (pressed || selected) ? pal.bg : color;
  gfx->startWrite();
  gfx->fillRect(x, y, TILE_W, TILE_H, face);
  if (!pressed && !selected) {
    gfx->drawRect(x, y, TILE_W, TILE_H, color);
    gfx->drawRect(x + 1, y + 1, TILE_W - 2, TILE_H - 2, color);
  }
  drawAppIcon(i, x + TILE_W / 2, y + 22, ink, face);
  gfx->setFont(FONT_BODY);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setTextColor(ink, face);
  gfx->drawString(APP_NAMES[i], x + TILE_W / 2, y + 41);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

static void drawMenu(int currentApp) {
  gfx->fillScreen(pal.bg);
  drawHeader("MENU", "SWIPE UP TO GO BACK");
  for (int i = 0; i < APP_COUNT; ++i) drawMenuTile(i, i == currentApp, false);
}

// ---------------------------------------------------------------- CLOCK
static const char* const DAY_NAMES[7]    = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };
static const char* const MONTH_NAMES[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
static const int CLOCK_TIME_Y = 40, CLOCK_TIME_H = 100, CLOCK_GAUGE_Y = 142, CLOCK_DATE_Y = 192;

static void clockParts(uint32_t epoch, struct tm& out) {
  time_t t = (time_t)epoch;
  gmtime_r(&t, &out);        // the computer already sent local time
}

static void drawClockTime(const ClockState& c) {
  struct tm tmv; clockParts(c.epoch, tmv);
  int h = tmv.tm_hour;
  const char* ampm = nullptr;
  if (!c.h24) { ampm = h >= 12 ? "PM" : "AM"; h %= 12; if (h == 0) h = 12; }
  char t[8];
  snprintf(t, sizeof t, c.h24 ? "%02d:%02d" : "%d:%02d", h, tmv.tm_min);

  gfx->startWrite();
  gfx->fillRect(0, CLOCK_TIME_Y, SCREEN_W, CLOCK_TIME_H, pal.bg);
  gfx->setFont(&fonts::Font7);
  gfx->setTextSize(2);
  int tw = gfx->textWidth(t);
  gfx->setFont(FONT_BIG);
  gfx->setTextSize(1);
  int aw = ampm ? gfx->textWidth(ampm) + 8 : 0;
  int lead = t[0] == '1' ? 40 : 0;          // a leading 1 is drawn at the right of its space; skip the empty part
  int x = (SCREEN_W - (tw - lead) - aw) / 2 - lead;

  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(&fonts::Font7);
  gfx->setTextSize(2);
  gfx->setTextColor(pal.cream, pal.bg);
  gfx->drawString(t, x, CLOCK_TIME_Y);
  gfx->setTextSize(1);
  if (ampm) {
    gfx->setFont(FONT_BIG);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString(ampm, x + tw + 8, CLOCK_TIME_Y + 72);
  }
  gfx->endWrite();
}

// One gauge segment per second of the current minute.
static void drawClockSeconds(const ClockState& c) {
  struct tm tmv; clockParts(c.epoch, tmv);
  LGFX_Sprite& b = *barSprite;
  b.fillScreen(pal.bg);
  for (int i = 0; i < 60; ++i)
    b.fillRect(10 + i * 5, 6, 4, (i % 5 == 0) ? 16 : 12, i <= tmv.tm_sec ? pal.accent : pal.rust);
  char t[8];
  snprintf(t, sizeof t, ":%02d", tmv.tm_sec);
  b.setFont(FONT_BODY);
  b.setTextDatum(lgfx::textdatum_t::top_right);
  b.setTextColor(pal.dim, pal.bg);
  b.drawString(t, SCREEN_W - 10, 26);
  b.pushSprite(0, CLOCK_GAUGE_Y);
}

static void drawClockDate(const ClockState& c) {
  struct tm tmv; clockParts(c.epoch, tmv);
  char d[40];
  snprintf(d, sizeof d, "%s  %s %d", DAY_NAMES[tmv.tm_wday % 7], MONTH_NAMES[tmv.tm_mon % 12], tmv.tm_mday);
  gfx->startWrite();
  gfx->fillRect(0, CLOCK_DATE_Y, SCREEN_W, SCREEN_H - CLOCK_DATE_Y, pal.bg);
  gfx->setFont(FONT_BIG);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(d, SCREEN_W / 2, CLOCK_DATE_Y);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

// ---- Full color look: the time sits on the animated sky, with the sun or moon crossing it.
static bool clockImmersive(const ClockState& c) { return fullColorMode && clockSkyOn && sceneReady && c.known; }

static void clockTimeText(const ClockState& c, char* time, size_t n, const char*& ampm) {
  struct tm tmv; clockParts(c.epoch, tmv);
  int h = tmv.tm_hour;
  ampm = "";
  if (!c.h24) { ampm = h >= 12 ? "PM" : "AM"; h %= 12; if (h == 0) h = 12; }
  snprintf(time, n, c.h24 ? "%02d:%02d" : "%d:%02d", h, tmv.tm_min);
}

static void drawClockSky(const ClockState& c, const Weather& w, bool advance) {
  SceneInfo info = skyInfo(w, c);
  sunOnArc(w, c, info);
  sceneRender(info, advance);

  struct tm tmv; clockParts(c.epoch, tmv);
  char t[8], sec[6], d[40];
  const char* ampm;
  clockTimeText(c, t, sizeof t, ampm);
  snprintf(sec, sizeof sec, ":%02d", tmv.tm_sec);
  snprintf(d, sizeof d, "%s  %s %d", DAY_NAMES[tmv.tm_wday % 7], MONTH_NAMES[tmv.tm_mon % 12], tmv.tm_mday);
  sceneClockWords(t, ampm, sec, d);
  sceneShow();
}

static void drawClockScreen(const ClockState& c) {
  if (!c.known) {
    drawMessageScreen("NO SIGNAL", "The clock starts when the", "computer connects.", MENU_HINT);
    return;
  }
  gfx->fillScreen(pal.bg);
  drawHeader("CLOCK");
  drawClockTime(c);
  drawClockSeconds(c);
  drawClockDate(c);
}

// ---------------------------------------------------------------- WEATHER
static void drawStatCell(int x, int y, const char* label, const char* value, uint32_t valueCol) {
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->drawString(label, x, y);
  gfx->setFont(FONT_TITLE);
  gfx->setTextColor(valueCol, pal.bg);
  gfx->drawString(value, x, y + 12);
}

// Minimal look: an icon, the temperature and the conditions.
static void drawWeatherNow(const Weather& w) {
  char buf[40];
  drawWeatherIcon(w.icon, 46, 64, 64, pal.accent, pal.cream, pal.bg);
  gfx->setFont(&fonts::Font7);
  gfx->setTextColor(pal.cream, pal.bg);
  gfx->drawString(w.temp, 96, 36);
  int tx = 96 + gfx->textWidth(w.temp) + 10;
  gfx->drawCircle(tx, 42, 5, pal.accent);
  gfx->drawCircle(tx, 42, 4, pal.accent);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(w.unit, tx + 9, 38);

  gfx->setFont(FONT_TITLE);
  gfx->setTextColor(pal.cream, pal.bg);
  drawWrapped(w.cond, 12, 102, 190, 1, 20);
  snprintf(buf, sizeof buf, "H %s   L %s", w.hi, w.lo);
  gfx->setFont(FONT_BODY);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(buf, SCREEN_W - 10, 104);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
}

// ---- Full color look: the whole screen is the animated sky. Swiping up shows the details
// ---- on a blurred copy of that sky.

// Draws the sky. advance = true also moves the clouds, rain and so on by one step.
static void drawWeatherSky(const Weather& w, const ClockState& c, bool advance) {
  sceneRender(skyInfo(w, c), advance);
  sceneWeatherWords(w.city, w.temp, w.unit, w.cond, w.hi, w.lo);
  sceneShow();
}

static void drawWeatherDetails(const Weather& w, const ClockState& c) {
  static const uint32_t WHITE = 0xFFFFFF, SOFT = 0xC9D6E6, LINE = 0x7C8A9E, ICON_BG = 0x151B26;
  char buf[40];

  // the backdrop: the sky alone, out of focus and darkened
  sceneRender(skyInfo(w, c), false);
  if (!sceneBlurToScreen(45)) gfx->fillScreen(pal.bg);

  gfx->startWrite();
  gfx->fillRect(SCREEN_W / 2 - 22, 1, 44, 3, SOFT);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(SOFT);
  gfx->drawString(w.city, 12, 12);
  snprintf(buf, sizeof buf, "UPDATED %s", w.updated);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  if (w.updated[0]) gfx->drawString(buf, SCREEN_W - 10, 12);

  // now
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(WHITE);
  gfx->drawString(w.temp, 12, 32);
  int x = 12 + gfx->textWidth(w.temp) + 7;
  gfx->drawCircle(x, 36, 3, WHITE);
  gfx->drawCircle(x, 36, 4, WHITE);
  gfx->drawString(w.unit, x + 7, 32);
  x += 7 + gfx->textWidth(w.unit) + 14;
  gfx->setFont(FONT_TITLE);
  drawWrapped(w.cond, x, 35, SCREEN_W - x - 10, 1, 20);
  gfx->drawFastHLine(10, 64, SCREEN_W - 20, LINE);

  // details
  static const char* const LABELS[4] = { "FEELS LIKE", "WIND", "HUMIDITY", "HIGH / LOW" };
  snprintf(buf, sizeof buf, "%s / %s", w.hi, w.lo);
  const char* values[4] = { w.feels, w.wind, w.hum, buf };
  for (int i = 0; i < 4; ++i) {
    int cx = 12 + (i % 2) * 156, cy = 74 + (i / 2) * 36;
    gfx->setFont(FONT_SMALL);
    gfx->setTextColor(SOFT);
    gfx->drawString(LABELS[i], cx, cy);
    gfx->setFont(FONT_TITLE);
    gfx->setTextColor(WHITE);
    gfx->drawString(values[i], cx, cy + 11);
  }
  gfx->drawFastHLine(10, 148, SCREEN_W - 20, LINE);

  // next days
  for (int i = 0; i < 3; ++i) {
    const WeatherDay& d = w.days[i];
    if (!d.name[0]) continue;
    int dx = 8 + i * 104;
    drawWeatherIcon(d.icon, dx + 15, 182, 30, 0xFFD23F, WHITE, ICON_BG);
    gfx->setFont(FONT_SMALL);
    gfx->setTextColor(SOFT);
    gfx->drawString(d.name, dx + 34, 164);
    snprintf(buf, sizeof buf, "%s / %s", d.hi, d.lo);
    gfx->setFont(FONT_TITLE);
    gfx->setTextColor(WHITE);
    gfx->drawString(buf, dx + 34, 176);
  }

  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(SOFT);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  gfx->drawString("SWIPE DOWN FOR THE SKY", SCREEN_W - 10, 226);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

// True when the Weather app should be the full-screen sky.
static bool weatherImmersive(const Weather& w) { return fullColorMode && sceneReady && w.valid; }

static void drawWeatherScreen(const Weather& w, const ClockState& c) {
  if (!w.valid) {
    drawMessageScreen("WEATHER", w.error, nullptr, MENU_HINT);
    return;
  }
  if (weatherImmersive(w)) {
    drawWeatherSky(w, c, false);
    return;
  }
  char buf[40];
  gfx->startWrite();
  gfx->fillScreen(pal.bg);
  snprintf(buf, sizeof buf, "UPDATED %s", w.updated);
  drawHeader(w.city[0] ? w.city : "WEATHER", w.updated[0] ? buf : nullptr);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);

  drawWeatherNow(w);
  gfx->drawFastHLine(8, 126, SCREEN_W - 16, pal.dim);

  // details
  drawStatCell(12, 133, "FEELS LIKE", w.feels, pal.accent);
  drawStatCell(116, 133, "WIND", w.wind, pal.accent);
  drawStatCell(220, 133, "HUMIDITY", w.hum, pal.accent);

  // next days
  gfx->drawFastHLine(8, 172, SCREEN_W - 16, pal.dim);
  for (int i = 0; i < 3; ++i) {
    const WeatherDay& d = w.days[i];
    if (!d.name[0]) continue;
    int x = 12 + i * 104;
    drawWeatherIcon(d.icon, x + 14, 210, 28, pal.accent, pal.cream, pal.bg);
    snprintf(buf, sizeof buf, "%s / %s", d.hi, d.lo);
    drawStatCell(x + 34, 192, d.name, buf, pal.cream);
  }
  gfx->endWrite();
}

// ---------------------------------------------------------------- REMINDERS
static const int REM_ROW_Y = 28, REM_ROW_H = 36, REM_FOOT_Y = REM_ROW_Y + REM_ROWS * REM_ROW_H;

static int reminderPages(const ReminderList& r) { return r.count <= REM_ROWS ? 1 : (r.count + REM_ROWS - 1) / REM_ROWS; }

// Which reminder a touch landed on. -1 = none, -2 = the footer (next page).
static int reminderAt(const ReminderList& r, int page, int y) {
  if (y >= REM_FOOT_Y) return -2;
  if (y < REM_ROW_Y) return -1;
  int i = page * REM_ROWS + (y - REM_ROW_Y) / REM_ROW_H;
  return i < r.count ? i : -1;
}

static void drawReminderRow(const ReminderList& r, int page, int i) {
  int row = i - page * REM_ROWS;
  if (row < 0 || row >= REM_ROWS) return;
  int y = REM_ROW_Y + row * REM_ROW_H;
  gfx->startWrite();
  gfx->fillRect(0, y, SCREEN_W, REM_ROW_H, pal.bg);
  if (i < r.count) {
    const Reminder& it = r.items[i];
    uint32_t col = it.done ? pal.dim : pal.cream;
    int bx = 12, by = y + 9;
    if (it.done) {
      gfx->fillRect(bx, by, 18, 18, pal.dim);
      thickLine(bx + 4, by + 9, bx + 7, by + 13, pal.bg);
      thickLine(bx + 7, by + 13, bx + 14, by + 4, pal.bg);
    } else {
      gfx->drawRect(bx, by, 18, 18, pal.accent);
      gfx->drawRect(bx + 1, by + 1, 16, 16, pal.accent);
    }
    int right = SCREEN_W - 10;
    gfx->setFont(FONT_BODY);
    if (it.time[0]) {
      gfx->setTextDatum(lgfx::textdatum_t::top_right);
      gfx->setTextColor(it.done ? pal.dim : pal.accent, pal.bg);
      gfx->drawString(it.time, right, y + 10);
      right -= gfx->textWidth(it.time) + 10;
      gfx->setTextDatum(lgfx::textdatum_t::top_left);
    }
    gfx->setTextColor(col, pal.bg);
    drawWrapped(it.text, 40, y + 10, right - 40, 1, BODY_LINE_H);
    gfx->drawFastHLine(8, y + REM_ROW_H - 1, SCREEN_W - 16, pal.rust);
  }
  gfx->endWrite();
}

static void drawRemindersScreen(const ReminderList& r, int page) {
  if (!r.valid) {
    drawMessageScreen("REMINDERS", "Waiting for the computer.", nullptr, MENU_HINT);
    return;
  }
  char buf[40];
  gfx->fillScreen(pal.bg);
  snprintf(buf, sizeof buf, "%d TO DO", r.open);
  drawHeader("REMINDERS", buf);

  if (r.count == 0) {
    gfx->setTextDatum(lgfx::textdatum_t::top_center);
    gfx->setFont(FONT_BIG);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString("ALL CLEAR", SCREEN_W / 2, 90);
    gfx->setFont(FONT_BODY);
    gfx->drawString("Add lines to reminders.txt", SCREEN_W / 2, 128);
    gfx->drawString("on the computer.", SCREEN_W / 2, 146);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
    return;
  }
  for (int row = 0; row < REM_ROWS; ++row) drawReminderRow(r, page, page * REM_ROWS + row);

  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->drawString("TAP TO CHECK OFF", 12, REM_FOOT_Y + 12);
  int pages = reminderPages(r);
  if (pages > 1) {
    snprintf(buf, sizeof buf, "PAGE %d/%d  TAP HERE FOR MORE", page + 1, pages);
    gfx->setTextDatum(lgfx::textdatum_t::top_right);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString(buf, SCREEN_W - 10, REM_FOOT_Y + 12);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
  }
}

// ---------------------------------------------------------------- CALENDAR
static int eventPages(const EventList& e) { return e.count <= LIST_ROWS ? 1 : (e.count + LIST_ROWS - 1) / LIST_ROWS; }

static void drawListFooter(const char* left, int page, int pages) {
  char buf[40];
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->drawString(left, 12, REM_FOOT_Y + 12);
  if (pages > 1) {
    snprintf(buf, sizeof buf, "PAGE %d/%d  TAP HERE FOR MORE", page + 1, pages);
    gfx->setTextDatum(lgfx::textdatum_t::top_right);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString(buf, SCREEN_W - 10, REM_FOOT_Y + 12);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
  }
}

static void drawCalendarScreen(const EventList& e, int page) {
  if (!e.valid) {
    drawMessageScreen("CALENDAR", e.error, nullptr, MENU_HINT);
    return;
  }
  gfx->fillScreen(pal.bg);
  drawHeader("CALENDAR", "FROM NOTION");
  if (e.count == 0) {
    gfx->setTextDatum(lgfx::textdatum_t::top_center);
    gfx->setFont(FONT_BIG);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString("ALL CLEAR", SCREEN_W / 2, 90);
    gfx->setFont(FONT_BODY);
    gfx->drawString("Nothing in the next 7 days.", SCREEN_W / 2, 128);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
    return;
  }
  gfx->startWrite();
  for (int row = 0; row < LIST_ROWS; ++row) {
    int i = page * LIST_ROWS + row;
    if (i >= e.count) break;
    const CalEvent& it = e.items[i];
    int y = REM_ROW_Y + row * REM_ROW_H;
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
    gfx->setFont(FONT_SMALL);
    gfx->setTextColor(pal.dim, pal.bg);
    gfx->drawString(it.day, 12, y + 5);
    gfx->setFont(FONT_BODY);
    gfx->setTextColor(pal.accent, pal.bg);
    gfx->drawString(it.time, 12, y + 15);
    gfx->setTextColor(pal.cream, pal.bg);
    drawWrapped(it.title, 92, y + 10, SCREEN_W - 92 - 10, 1, BODY_LINE_H);
    gfx->drawFastHLine(8, y + REM_ROW_H - 1, SCREEN_W - 16, pal.rust);
  }
  gfx->endWrite();
  drawListFooter("NEXT 7 DAYS", page, eventPages(e));
}

// ---------------------------------------------------------------- SYSTEM
static const int SYS_ROW_Y = 32, SYS_ROW_H = 28, SYS_GAUGE_X = 84, SYS_GAUGE_SEGS = 22;

static void fmtTemp(char* out, size_t n, int celsius, bool fahrenheit) {
  if (celsius == STAT_UNKNOWN) { snprintf(out, n, "N/A"); return; }
  if (fahrenheit) snprintf(out, n, "%d F", (celsius * 9 + (celsius >= 0 ? 2 : -2)) / 5 + 32);
  else            snprintf(out, n, "%d C", celsius);
}

static void drawSystemGauge(int row, const char* label, int percent, bool known) {
  int y = SYS_ROW_Y + row * SYS_ROW_H;
  char buf[12];
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->drawString(label, 12, y + 8);
  int lit = known ? (percent * SYS_GAUGE_SEGS + 50) / 100 : 0;
  for (int i = 0; i < SYS_GAUGE_SEGS; ++i) {
    bool hot = i >= SYS_GAUGE_SEGS * 85 / 100;            // the last stretch of the gauge is the red line
    gfx->fillRect(SYS_GAUGE_X + i * 8, y + 5, 6, 14, i < lit ? (hot ? pal.cream : pal.accent) : pal.rust);
  }
  gfx->fillRect(SYS_GAUGE_X + SYS_GAUGE_SEGS * 8 + 2, y, SCREEN_W - (SYS_GAUGE_X + SYS_GAUGE_SEGS * 8 + 2), SYS_ROW_H - 2, pal.bg);
  if (known) snprintf(buf, sizeof buf, "%d%%", percent); else snprintf(buf, sizeof buf, "--");
  gfx->setFont(FONT_TITLE);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(buf, SCREEN_W - 10, y + 5);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
}

static void drawSystemCell(int x, int y, const char* label, const char* value) {
  gfx->fillRect(x, y + 11, 100, 20, pal.bg);
  drawStatCell(x, y, label, value, pal.accent);
}

// The numbers only. Called again whenever they change, so nothing flickers.
static void drawSystemValues(const Stats& st, const Settings& set) {
  char buf[20];
  gfx->startWrite();
  drawSystemGauge(0, "CPU", st.cpu, st.pcValid);
  drawSystemGauge(1, "MEMORY", st.mem, st.pcValid);
  drawSystemGauge(2, "DISK", st.disk, st.pcValid);

  fmtTemp(buf, sizeof buf, st.pcValid ? st.cpuTemp : STAT_UNKNOWN, set.statsF);
  drawSystemCell(12, 124, "CPU TEMP", buf);
  fmtTemp(buf, sizeof buf, st.pcValid ? st.gpuTemp : STAT_UNKNOWN, set.statsF);
  drawSystemCell(116, 124, "GRAPHICS TEMP", buf);
  drawSystemCell(220, 124, "COMPUTER", st.linkUp ? "ONLINE" : "OFFLINE");

  fmtTemp(buf, sizeof buf, st.chipTemp, set.statsF);
  drawSystemCell(12, 186, "CHIP TEMP", buf);
  snprintf(buf, sizeof buf, "%u:%02u:%02u", (unsigned)(st.uptimeSec / 3600), (unsigned)((st.uptimeSec / 60) % 60), (unsigned)(st.uptimeSec % 60));
  drawSystemCell(116, 186, "RUNNING FOR", buf);
  snprintf(buf, sizeof buf, "%u KB", (unsigned)st.freeKb);
  drawSystemCell(220, 186, "FREE MEMORY", buf);
  gfx->endWrite();
}

static void drawSystemScreen(const Stats& st, const Settings& set) {
  gfx->fillScreen(pal.bg);
  drawHeader("SYSTEM", "COMPUTER");
  gfx->drawFastHLine(8, 118, SCREEN_W - 16, pal.dim);
  gfx->drawFastHLine(8, 162, SCREEN_W - 16, pal.dim);
  gfx->setFont(FONT_SMALL);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->drawString("THIS DISPLAY", SCREEN_W - 8, 168);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  drawSystemValues(st, set);
}

// ---------------------------------------------------------------- SETTINGS
static const int SETTING_COUNT = 14;
enum SettingRow { SET_LOOK, SET_ALBUM_COLORS, SET_FULL_ART, SET_VISUALIZER, SET_TIME, SET_CLOCK_SKY, SET_WEATHER_UNIT,
                  SET_SUN_GLOW, SET_REMINDER_ALERTS, SET_EVENT_ALERTS, SET_STATS_UNIT, SET_BRIGHTNESS, SET_SAVER, SET_LIGHT };
static const char* const SETTING_APP[SETTING_COUNT]  = {
  "DISPLAY", "MUSIC, IN THE FULL COLOR LOOK", "MUSIC", "MUSIC, ON THE FULL-SCREEN COVER", "CLOCK",
  "CLOCK, IN THE FULL COLOR LOOK", "WEATHER", "WEATHER, CLOCK, SCREENSAVER", "REMINDERS", "CALENDAR", "SYSTEM",
  "DISPLAY", "DISPLAY", "DISPLAY",
};
static const char* const SETTING_NAME[SETTING_COUNT] = {
  "Look", "Album colors", "Full-screen cover", "Visualizer bars", "Time format", "Moving sky", "Temperature",
  "Sunrise and sunset colors", "Timed alerts", "Alert before events", "Temperature", "Brightness", "Screensaver",
  "Alert light on the back",
};
static const uint8_t SAVER_MINUTES[4] = { 0, 1, 5, 15 };

static int settingsPages() { return (SETTING_COUNT + LIST_ROWS - 1) / LIST_ROWS; }

// The text in the box at the right of a row. 'on' says whether to draw it as a lit switch.
static void settingValue(const Settings& s, int i, char* out, size_t n, bool& isSwitch, bool& on) {
  isSwitch = true; on = false;
  switch (i) {
    case SET_LOOK:            isSwitch = false; snprintf(out, n, s.fullColor ? "FULL COLOR" : "MINIMAL"); return;
    case SET_ALBUM_COLORS:    on = s.albumColors; break;
    case SET_FULL_ART:
      if (s.cover) { isSwitch = false; snprintf(out, n, s.cover == 2 ? "AUTO" : "ON TAP"); return; }
      break;                                                    // off: drawn as a switch that is off
    case SET_VISUALIZER:      on = s.visualizer; break;
    case SET_CLOCK_SKY:       on = s.clockSky; break;
    case SET_SUN_GLOW:        on = s.sunGlow; break;
    case SET_TIME:            isSwitch = false; snprintf(out, n, s.h24 ? "24 H" : "12 H"); return;
    case SET_WEATHER_UNIT:    isSwitch = false; snprintf(out, n, s.weatherC ? "C" : "F"); return;
    case SET_REMINDER_ALERTS: on = s.reminderAlerts; break;
    case SET_EVENT_ALERTS:    on = s.eventAlerts; break;
    case SET_STATS_UNIT:      isSwitch = false; snprintf(out, n, s.statsF ? "F" : "C"); return;
    case SET_BRIGHTNESS:      isSwitch = false; snprintf(out, n, "%d%%", (s.brightness + 1) * 25); return;
    case SET_SAVER:
      if (s.saver & 3) { isSwitch = false; snprintf(out, n, "%d MIN", SAVER_MINUTES[s.saver & 3]); return; }
      break;                                                    // off: drawn as a switch that is off
    case SET_LIGHT:           on = s.alertLight; break;
  }
  snprintf(out, n, on ? "ON" : "OFF");
}

// Tapping a row moves it to its next value.
static void settingAdvance(Settings& s, int i) {
  switch (i) {
    case SET_LOOK:            s.fullColor = !s.fullColor; break;
    case SET_ALBUM_COLORS:    s.albumColors = !s.albumColors; break;
    case SET_FULL_ART:        s.cover = (s.cover + 2) % 3; break;      // AUTO -> ON TAP -> OFF -> AUTO
    case SET_VISUALIZER:      s.visualizer = !s.visualizer; break;
    case SET_CLOCK_SKY:       s.clockSky = !s.clockSky; break;
    case SET_SUN_GLOW:        s.sunGlow = !s.sunGlow; break;
    case SET_TIME:            s.h24 = !s.h24; break;
    case SET_WEATHER_UNIT:    s.weatherC = !s.weatherC; break;
    case SET_REMINDER_ALERTS: s.reminderAlerts = !s.reminderAlerts; break;
    case SET_EVENT_ALERTS:    s.eventAlerts = !s.eventAlerts; break;
    case SET_STATS_UNIT:      s.statsF = !s.statsF; break;
    case SET_BRIGHTNESS:      s.brightness = (s.brightness + 1) % 4; break;
    case SET_SAVER:           s.saver = (s.saver + 1) % 4; break;
    case SET_LIGHT:           s.alertLight = !s.alertLight; break;
  }
}

static int settingAt(int page, int y) {            // -1 = none, -2 = the footer (next page)
  if (y >= REM_FOOT_Y) return -2;
  if (y < REM_ROW_Y) return -1;
  int i = page * LIST_ROWS + (y - REM_ROW_Y) / REM_ROW_H;
  return i < SETTING_COUNT ? i : -1;
}

static void drawSettingsRow(const Settings& s, int page, int i) {
  int row = i - page * LIST_ROWS;
  if (row < 0 || row >= LIST_ROWS || i >= SETTING_COUNT) return;
  int y = REM_ROW_Y + row * REM_ROW_H;
  char val[14]; bool isSwitch, on;
  settingValue(s, i, val, sizeof val, isSwitch, on);

  gfx->startWrite();
  gfx->fillRect(0, y, SCREEN_W, REM_ROW_H, pal.bg);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->drawString(SETTING_APP[i], 12, y + 5);
  gfx->setFont(FONT_BODY);
  gfx->setTextColor(pal.cream, pal.bg);
  gfx->drawString(SETTING_NAME[i], 12, y + 15);

  gfx->setFont(FONT_TITLE);
  int bw = gfx->textWidth(val) + 16;
  if (bw < 62) bw = 62;
  int bh = 24, bx = SCREEN_W - 10 - bw, by = y + 6;
  uint32_t face = (isSwitch && on) ? pal.accent : pal.bg;
  uint32_t ink  = (isSwitch && on) ? pal.bg : ((isSwitch && !on) ? pal.dim : pal.accent);
  gfx->fillRect(bx, by, bw, bh, face);
  if (face == pal.bg) {
    gfx->drawRect(bx, by, bw, bh, ink);
    gfx->drawRect(bx + 1, by + 1, bw - 2, bh - 2, ink);
  }
  gfx->setFont(FONT_TITLE);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setTextColor(ink);
  gfx->drawString(val, bx + bw / 2, by + 4);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->drawFastHLine(8, y + REM_ROW_H - 1, SCREEN_W - 16, pal.rust);
  gfx->endWrite();
}

static void drawSettingsScreen(const Settings& s, int page) {
  gfx->fillScreen(pal.bg);
  drawHeader("SETTINGS");
  for (int row = 0; row < LIST_ROWS; ++row) drawSettingsRow(s, page, page * LIST_ROWS + row);
  drawListFooter("TAP TO CHANGE", page, settingsPages());
}

// ---------------------------------------------------------------- SCREENSAVER
// With no music playing: the sky for the current weather and time of day (stars at night, rain
// when it rains) with a small clock drifting around it. In the Minimal look the sky is black
// with amber stars. While music plays the cover floats around instead (see ui.h).
struct SaverState { float x = 60, y = 70, dx = 0.55f, dy = 0.35f; int shownX = -1, shownY = -1; };

static void saverMove(SaverState& sv) {
  sv.x += sv.dx; sv.y += sv.dy;
  if (sv.x < 6 || sv.x > SCREEN_W - SAVER_BOX_W - 6) { sv.dx = -sv.dx; sv.x += 2 * sv.dx; }
  if (sv.y < 6 || sv.y > SCREEN_H - SAVER_BOX_H - 6) { sv.dy = -sv.dy; sv.y += 2 * sv.dy; }
}

static void saverWords(const ClockState& c, char* t, size_t tn, char* d, size_t dn) {
  struct tm tmv; clockParts(c.epoch, tmv);
  const char* ampm;
  clockTimeText(c, t, tn, ampm);
  snprintf(d, dn, "%s  %s %d  %s", DAY_NAMES[tmv.tm_wday % 7], MONTH_NAMES[tmv.tm_mon % 12], tmv.tm_mday, ampm);
}

static void drawSaverSky(const ClockState& c, const Weather& w, SaverState& sv, bool advance) {
  if (advance) saverMove(sv);
  char t[8], d[40];
  if (!sceneReady) {                                   // no room for the sky picture: just the drifting clock
    int x = (int)sv.x, y = (int)sv.y;
    if (!advance) gfx->fillScreen(COL_BG);
    else if (x == sv.shownX && y == sv.shownY) return;
    else if (sv.shownX >= 0) gfx->fillRect(sv.shownX, sv.shownY, SAVER_BOX_W, SAVER_BOX_H, COL_BG);
    sv.shownX = x; sv.shownY = y;
    if (!c.known) return;
    saverWords(c, t, sizeof t, d, sizeof d);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
    gfx->setFont(&fonts::Font7);
    gfx->setTextColor(COL_AMBER);
    gfx->drawString(t, x, y);
    gfx->setFont(FONT_SMALL);
    gfx->setTextColor(COL_AMBER_DIM);
    gfx->drawString(d, x + 2, y + 58);
    return;
  }
  SceneInfo info = skyInfo(w, c);
  sunOnArc(w, c, info);
  if (!fullColorMode) { info.kind = 0; info.plain = true; info.plainInk = COL_AMBER; }
  sceneRender(info, advance);
  if (c.known) {
    saverWords(c, t, sizeof t, d, sizeof d);
    sceneSaverWords(t, d, (int)sv.x, (int)sv.y);
  }
  sceneShow();
}

// ---------------------------------------------------------------- REMINDER ALERT
static void drawAlert(const Alert& a) {
  gfx->startWrite();
  gfx->fillScreen(pal.bg);
  drawHazardBand(0, 16);
  drawHazardBand(SCREEN_H - 16, 16);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(a.time[0] ? a.time : "REMINDER", SCREEN_W / 2, 40);
  gfx->drawFastHLine(60, 72, SCREEN_W - 120, pal.dim);
  gfx->setTextColor(pal.cream, pal.bg);
  drawWrapped(a.text, SCREEN_W / 2, 88, SCREEN_W - 40, 3, 28);
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(pal.dim, pal.bg);
  gfx->drawString("TAP TO DISMISS", SCREEN_W / 2, 200);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}
