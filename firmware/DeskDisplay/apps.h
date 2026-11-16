// apps.h - the menu and the Clock, Weather and Reminders screens.
// Uses the colors, fonts and helpers from ui.h.
#pragma once
#include "ui.h"
#include <time.h>
#include <stdlib.h>

enum App { APP_MUSIC = 0, APP_CLOCK = 1, APP_WEATHER = 2, APP_REMINDERS = 3, APP_COUNT = 4 };

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

// "OK" city temp unit condition icon high low feels wind humidity updated, one per line,
// then up to three lines of "day|icon|high|low".   Or: "ERR" and a message line.
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

static void parseAlert(const char* p, Alert& a) {
  takeLine(a.time, sizeof a.time, p);
  takeLine(a.text, sizeof a.text, p);
}

// ---------------------------------------------------------------- shared pieces
static void drawHeader(const char* title, const char* right = nullptr) {
  drawPullHandle();
  gfx->setFont(FONT_SMALL);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->fillRect(8, 12, 7, 7, COL_AMBER);
  gfx->setTextColor(COL_AMBER, COL_BG);
  gfx->drawString(title, 20, 12);
  if (right) {
    gfx->setTextDatum(lgfx::textdatum_t::top_right);
    gfx->setTextColor(COL_AMBER_DIM, COL_BG);
    gfx->drawString(right, SCREEN_W - 8, 12);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
  }
  gfx->drawFastHLine(8, 24, SCREEN_W - 16, COL_AMBER_DIM);
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
static const int TILE_W = 148, TILE_H = 92;
static const int TILE_X[2] = { 8, 164 };
static const int TILE_Y[2] = { 34, 136 };
static const char* const APP_NAMES[APP_COUNT] = { "MUSIC", "CLOCK", "WEATHER", "REMINDERS" };

static int menuTileAt(int x, int y) {
  if (y < TILE_Y[0] - 4) return -1;
  return (y < TILE_Y[1] - 5 ? 0 : 2) + (x < SCREEN_W / 2 ? 0 : 1);
}

static void drawMenuTile(int i, bool selected, bool pressed) {
  int x = TILE_X[i % 2], y = TILE_Y[i / 2];
  uint32_t face = pressed ? COL_CREAM : (selected ? COL_AMBER : COL_BG);
  uint32_t ink  = (pressed || selected) ? COL_BG : COL_AMBER;
  gfx->startWrite();
  gfx->fillRect(x, y, TILE_W, TILE_H, face);
  if (!pressed && !selected) {
    gfx->drawRect(x, y, TILE_W, TILE_H, COL_AMBER);
    gfx->drawRect(x + 1, y + 1, TILE_W - 2, TILE_H - 2, COL_AMBER);
  }
  int cx = x + TILE_W / 2, cy = y + 34;
  if (i == APP_MUSIC) {
    gfx->fillCircle(cx - 11, cy + 12, 7, ink);
    gfx->fillCircle(cx + 11, cy + 8, 7, ink);
    gfx->fillRect(cx - 7, cy - 14, 3, 27, ink);
    gfx->fillRect(cx + 15, cy - 18, 3, 27, ink);
    gfx->fillTriangle(cx - 7, cy - 14, cx + 18, cy - 18, cx + 18, cy - 11, ink);
    gfx->fillTriangle(cx - 7, cy - 14, cx - 7, cy - 7, cx + 18, cy - 11, ink);
  } else if (i == APP_CLOCK) {
    gfx->fillCircle(cx, cy, 19, ink);
    gfx->fillCircle(cx, cy, 15, face);
    gfx->fillRect(cx - 1, cy - 11, 3, 13, ink);
    gfx->fillRect(cx - 1, cy - 1, 10, 3, ink);
  } else if (i == APP_WEATHER) {
    drawWeatherIcon(1, cx, cy, 44, ink, ink, face);
  } else {
    for (int row = 0; row < 2; ++row) {
      int ry = cy - 15 + row * 20;
      gfx->drawRect(cx - 24, ry, 12, 12, ink);
      gfx->drawRect(cx - 23, ry + 1, 10, 10, ink);
      gfx->fillRect(cx - 6, ry + 4, 30, 4, ink);
    }
    thickLine(cx - 21, cy - 10, cx - 19, cy - 7, ink);
    thickLine(cx - 19, cy - 7, cx - 15, cy - 13, ink);
  }
  gfx->setFont(FONT_TITLE);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setTextColor(ink, face);
  gfx->drawString(APP_NAMES[i], cx, y + 64);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

static void drawMenu(int currentApp) {
  gfx->fillScreen(COL_BG);
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
  gfx->fillRect(0, CLOCK_TIME_Y, SCREEN_W, CLOCK_TIME_H, COL_BG);
  gfx->setFont(&fonts::Font7);
  gfx->setTextSize(2);
  int tw = gfx->textWidth(t);
  gfx->setFont(FONT_BIG);
  gfx->setTextSize(1);
  int aw = ampm ? gfx->textWidth(ampm) + 8 : 0;
  int x = (SCREEN_W - tw - aw) / 2;

  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->setFont(&fonts::Font7);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_CREAM, COL_BG);
  gfx->drawString(t, x, CLOCK_TIME_Y);
  gfx->setTextSize(1);
  if (ampm) {
    gfx->setFont(FONT_BIG);
    gfx->setTextColor(COL_AMBER, COL_BG);
    gfx->drawString(ampm, x + tw + 8, CLOCK_TIME_Y + 72);
  }
  gfx->endWrite();
}

// One gauge segment per second of the current minute.
static void drawClockSeconds(const ClockState& c) {
  struct tm tmv; clockParts(c.epoch, tmv);
  LGFX_Sprite& b = *barSprite;
  b.fillScreen(COL_BG);
  for (int i = 0; i < 60; ++i)
    b.fillRect(10 + i * 5, 6, 4, (i % 5 == 0) ? 16 : 12, i <= tmv.tm_sec ? COL_AMBER : COL_RUST);
  char t[8];
  snprintf(t, sizeof t, ":%02d", tmv.tm_sec);
  b.setFont(FONT_BODY);
  b.setTextDatum(lgfx::textdatum_t::top_right);
  b.setTextColor(COL_AMBER_DIM, COL_BG);
  b.drawString(t, SCREEN_W - 10, 26);
  b.pushSprite(0, CLOCK_GAUGE_Y);
}

static void drawClockDate(const ClockState& c) {
  struct tm tmv; clockParts(c.epoch, tmv);
  char d[40];
  snprintf(d, sizeof d, "%s  %s %d", DAY_NAMES[tmv.tm_wday % 7], MONTH_NAMES[tmv.tm_mon % 12], tmv.tm_mday);
  gfx->startWrite();
  gfx->fillRect(0, CLOCK_DATE_Y, SCREEN_W, SCREEN_H - CLOCK_DATE_Y, COL_BG);
  gfx->setFont(FONT_BIG);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setTextColor(COL_AMBER, COL_BG);
  gfx->drawString(d, SCREEN_W / 2, CLOCK_DATE_Y);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

static void drawClockScreen(const ClockState& c) {
  if (!c.known) {
    drawMessageScreen("NO SIGNAL", "The clock starts when the", "computer connects.", MENU_HINT);
    return;
  }
  gfx->fillScreen(COL_BG);
  drawHeader("CLOCK");
  drawClockTime(c);
  drawClockSeconds(c);
  drawClockDate(c);
}

// ---------------------------------------------------------------- WEATHER
static void drawStatCell(int x, int y, const char* label, const char* value, uint32_t valueCol) {
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(COL_AMBER_DIM, COL_BG);
  gfx->drawString(label, x, y);
  gfx->setFont(FONT_TITLE);
  gfx->setTextColor(valueCol, COL_BG);
  gfx->drawString(value, x, y + 12);
}

static void drawWeatherScreen(const Weather& w) {
  if (!w.valid) {
    drawMessageScreen("WEATHER", w.error, nullptr, MENU_HINT);
    return;
  }
  char buf[40];
  gfx->startWrite();
  gfx->fillScreen(COL_BG);
  snprintf(buf, sizeof buf, "UPDATED %s", w.updated);
  drawHeader(w.city[0] ? w.city : "WEATHER", w.updated[0] ? buf : nullptr);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);

  // now
  drawWeatherIcon(w.icon, 46, 64, 64, COL_AMBER, COL_CREAM, COL_BG);
  gfx->setFont(&fonts::Font7);
  gfx->setTextColor(COL_CREAM, COL_BG);
  gfx->drawString(w.temp, 96, 36);
  int tx = 96 + gfx->textWidth(w.temp) + 10;
  gfx->drawCircle(tx, 42, 5, COL_AMBER);
  gfx->drawCircle(tx, 42, 4, COL_AMBER);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(COL_AMBER, COL_BG);
  gfx->drawString(w.unit, tx + 9, 38);

  gfx->setFont(FONT_TITLE);
  gfx->setTextColor(COL_CREAM, COL_BG);
  drawWrapped(w.cond, 12, 102, 190, 1, 20);
  snprintf(buf, sizeof buf, "H %s   L %s", w.hi, w.lo);
  gfx->setFont(FONT_BODY);
  gfx->setTextDatum(lgfx::textdatum_t::top_right);
  gfx->setTextColor(COL_AMBER, COL_BG);
  gfx->drawString(buf, SCREEN_W - 10, 104);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);

  // details
  gfx->drawFastHLine(8, 126, SCREEN_W - 16, COL_AMBER_DIM);
  drawStatCell(12, 133, "FEELS LIKE", w.feels, COL_AMBER);
  drawStatCell(116, 133, "WIND", w.wind, COL_AMBER);
  drawStatCell(220, 133, "HUMIDITY", w.hum, COL_AMBER);

  // next days
  gfx->drawFastHLine(8, 172, SCREEN_W - 16, COL_AMBER_DIM);
  for (int i = 0; i < 3; ++i) {
    const WeatherDay& d = w.days[i];
    if (!d.name[0]) continue;
    int x = 12 + i * 104;
    drawWeatherIcon(d.icon, x + 14, 210, 28, COL_AMBER, COL_CREAM, COL_BG);
    snprintf(buf, sizeof buf, "%s / %s", d.hi, d.lo);
    drawStatCell(x + 34, 192, d.name, buf, COL_CREAM);
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
  gfx->fillRect(0, y, SCREEN_W, REM_ROW_H, COL_BG);
  if (i < r.count) {
    const Reminder& it = r.items[i];
    uint32_t col = it.done ? COL_AMBER_DIM : COL_CREAM;
    int bx = 12, by = y + 9;
    if (it.done) {
      gfx->fillRect(bx, by, 18, 18, COL_AMBER_DIM);
      thickLine(bx + 4, by + 9, bx + 7, by + 13, COL_BG);
      thickLine(bx + 7, by + 13, bx + 14, by + 4, COL_BG);
    } else {
      gfx->drawRect(bx, by, 18, 18, COL_AMBER);
      gfx->drawRect(bx + 1, by + 1, 16, 16, COL_AMBER);
    }
    int right = SCREEN_W - 10;
    gfx->setFont(FONT_BODY);
    if (it.time[0]) {
      gfx->setTextDatum(lgfx::textdatum_t::top_right);
      gfx->setTextColor(it.done ? COL_AMBER_DIM : COL_AMBER, COL_BG);
      gfx->drawString(it.time, right, y + 10);
      right -= gfx->textWidth(it.time) + 10;
      gfx->setTextDatum(lgfx::textdatum_t::top_left);
    }
    gfx->setTextColor(col, COL_BG);
    drawWrapped(it.text, 40, y + 10, right - 40, 1, BODY_LINE_H);
    gfx->drawFastHLine(8, y + REM_ROW_H - 1, SCREEN_W - 16, COL_RUST);
  }
  gfx->endWrite();
}

static void drawRemindersScreen(const ReminderList& r, int page) {
  if (!r.valid) {
    drawMessageScreen("REMINDERS", "Waiting for the computer.", nullptr, MENU_HINT);
    return;
  }
  char buf[40];
  gfx->fillScreen(COL_BG);
  snprintf(buf, sizeof buf, "%d TO DO", r.open);
  drawHeader("REMINDERS", buf);

  if (r.count == 0) {
    gfx->setTextDatum(lgfx::textdatum_t::top_center);
    gfx->setFont(FONT_BIG);
    gfx->setTextColor(COL_AMBER, COL_BG);
    gfx->drawString("ALL CLEAR", SCREEN_W / 2, 90);
    gfx->setFont(FONT_BODY);
    gfx->drawString("Add lines to reminders.txt", SCREEN_W / 2, 128);
    gfx->drawString("on the computer.", SCREEN_W / 2, 146);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
    return;
  }
  for (int row = 0; row < REM_ROWS; ++row) drawReminderRow(r, page, page * REM_ROWS + row);

  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(COL_AMBER_DIM, COL_BG);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->drawString("TAP TO CHECK OFF", 12, REM_FOOT_Y + 12);
  int pages = reminderPages(r);
  if (pages > 1) {
    snprintf(buf, sizeof buf, "PAGE %d/%d  TAP HERE FOR MORE", page + 1, pages);
    gfx->setTextDatum(lgfx::textdatum_t::top_right);
    gfx->setTextColor(COL_AMBER, COL_BG);
    gfx->drawString(buf, SCREEN_W - 10, REM_FOOT_Y + 12);
    gfx->setTextDatum(lgfx::textdatum_t::top_left);
  }
}

// ---------------------------------------------------------------- REMINDER ALERT
static void drawAlert(const Alert& a) {
  gfx->startWrite();
  gfx->fillScreen(COL_BG);
  drawHazardBand(0, 16);
  drawHazardBand(SCREEN_H - 16, 16);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(COL_AMBER, COL_BG);
  gfx->drawString(a.time[0] ? a.time : "REMINDER", SCREEN_W / 2, 40);
  gfx->drawFastHLine(60, 72, SCREEN_W - 120, COL_AMBER_DIM);
  gfx->setTextColor(COL_CREAM, COL_BG);
  drawWrapped(a.text, SCREEN_W / 2, 88, SCREEN_W - 40, 3, 28);
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(COL_AMBER_DIM, COL_BG);
  gfx->drawString("TAP TO DISMISS", SCREEN_W / 2, 200);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}
