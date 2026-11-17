// ui.h - everything that gets drawn on the 320 x 240 screen.
// To restyle the display, start with the THEME and LAYOUT sections below.
#pragma once
#include <LovyanGFX.hpp>
#include <string.h>
#include <stdio.h>
#include "protocol.h"

// ---------------------------------------------------------------- THEME
// Colors are ordinary 24-bit hex values (0xRRGGBB).
static const uint32_t COL_BG        = 0x0E0C0A;  // near-black
static const uint32_t COL_AMBER     = 0xEF9F27;  // main accent
static const uint32_t COL_AMBER_DIM = 0x854F0B;  // secondary text, outlines
static const uint32_t COL_RUST      = 0x412402;  // empty gauge segments, art placeholder
static const uint32_t COL_CREAM     = 0xFAEEDA;  // title text, scrub handle

#define FONT_TITLE   (&fonts::FreeSansBold9pt7b)
#define FONT_BODY    (&fonts::Font2)
#define FONT_SMALL   (&fonts::Font0)
#define FONT_BIG     (&fonts::FreeSansBold12pt7b)

// ---------------------------------------------------------------- LAYOUT
static const int SCREEN_W = 320, SCREEN_H = 240;

static const int ART_X = 8, ART_Y = 8;                    // album art, ART_SIZE square

static const int TEXT_X = 162, TEXT_W = 150;              // text column right of the art
static const int LABEL_Y = 8, TITLE_Y = 28;
static const int TITLE_LINE_H = 21, BODY_LINE_H = 16;

static const int BAR_AREA_Y = 148, BAR_AREA_H = 44;       // strip holding gauge + times
static const int BAR_X = 8, BAR_W = 304;                  // gauge, relative to the strip
static const int BAR_Y = 8, BAR_H = 12;
static const int SEG_PITCH = 8, SEG_W = 6;                // gauge segments

static const int BTN_Y = 196, BTN_H = 40, BTN_W = 96;
static const int BTN_X[3] = { 8, 112, 216 };              // prev, play/pause, next

enum Zone { ZONE_NONE = -1, ZONE_PREV = 0, ZONE_PLAY = 1, ZONE_NEXT = 2, ZONE_BAR = 3 };
enum View { VIEW_NOLINK, VIEW_STANDBY, VIEW_PLAYING };

struct UiState {
  char title[100]  = "";
  char artist[100] = "";
  char album[100]  = "";
  bool playing   = false;
  bool hasPos    = false;
  bool canSeek   = false;
  uint32_t posMs = 0;        // position to show right now
  uint32_t durMs = 0;
  bool artValid  = false;
  const uint8_t* art = nullptr;
  int  pressed   = ZONE_NONE; // button currently held down
};

static lgfx::LovyanGFX* gfx = nullptr;   // the screen
static LGFX_Sprite*     barSprite = nullptr;

// ---------------------------------------------------------------- palettes
// Nothing draws with the THEME colors directly. Screens draw with a palette:
//   pal       the palette of whatever screen is showing (menu, clock, weather, ...)
//   musicPal  the palette of the music screen
// In the Minimal look both are the THEME colors. In the Full color look each app gets its own
// palette, and the music screen's is rebuilt from each album cover.
struct Palette {
  uint32_t bg, accent, dim, rust, cream;
  bool operator==(const Palette& o) const { return bg == o.bg && accent == o.accent && dim == o.dim && rust == o.rust && cream == o.cream; }
};
static const Palette SYSTEM_PALETTE = { COL_BG, COL_AMBER, COL_AMBER_DIM, COL_RUST, COL_CREAM };
static Palette musicPal = SYSTEM_PALETTE;
static Palette pal = SYSTEM_PALETTE;

// h in 0..360, s and l in 0..1  ->  0xRRGGBB
static uint32_t hslColor(float h, float s, float l) {
  float c = (1 - (l * 2 > 1 ? l * 2 - 1 : 1 - l * 2)) * s;
  float hp = h / 60.0f;
  float t = hp - 2 * (int)(hp / 2);
  float x = c * (1 - (t > 1 ? t - 1 : 1 - t));
  float r = 0, g = 0, b = 0;
  switch (((int)hp) % 6) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
  }
  float m = l - c / 2;
  int R = (int)((r + m) * 255 + 0.5f), G = (int)((g + m) * 255 + 0.5f), B = (int)((b + m) * 255 + 0.5f);
  return ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
}

static float luma(uint32_t c) {
  return (0.299f * ((c >> 16) & 255) + 0.587f * ((c >> 8) & 255) + 0.114f * (c & 255)) / 255.0f;
}

// Same hue, but light enough to read: blues and purples need more lightness than yellows.
static uint32_t hslReadable(float h, float s, float l, float minLuma) {
  uint32_t c = hslColor(h, s, l);
  while (luma(c) < minLuma && l < 0.88f) { l += 0.03f; c = hslColor(h, s, l); }
  return c;
}

// A whole palette from one hue (0..360) and how colorful it should be (0..1):
// a dark background, a bright accent, and the tones in between.
static Palette paletteFromHS(float h, float s) {
  Palette p;
  p.bg     = hslColor(h, s * 0.60f, 0.09f);
  p.rust   = hslColor(h, s * 0.55f, 0.19f);
  p.dim    = hslReadable(h, s * 0.60f, 0.40f, 0.22f);
  p.accent = hslReadable(h, s * 0.90f, 0.62f, 0.50f);
  p.cream  = hslColor(h, s * 0.40f, 0.94f);
  return p;
}

// Finds the cover's main color and builds a dark background, a bright accent and the in-between
// tones from it. 'art' is ART_SIZE x ART_SIZE pixels of RGB565, high byte first.
static Palette paletteFromArt(const uint8_t* art) {
  static const int BINS = 24;
  uint32_t weight[BINS] = { 0 }, sumR[BINS] = { 0 }, sumG[BINS] = { 0 }, sumB[BINS] = { 0 };
  uint32_t samples = 0, colorful = 0;
  for (int y = 0; y < ART_SIZE; y += 2) {
    for (int x = 0; x < ART_SIZE; x += 2) {
      const uint8_t* p = art + ((size_t)y * ART_SIZE + x) * 2;
      uint16_t v = ((uint16_t)p[0] << 8) | p[1];
      int r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
      int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
      int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
      int chroma = mx - mn;
      ++samples;
      if (chroma < 45 || mx < 55) continue;          // gray, black or white: no hue to speak of
      float h;
      if (mx == r)      h = 60.0f * (g - b) / chroma + (g < b ? 360.0f : 0.0f);
      else if (mx == g) h = 60.0f * (b - r) / chroma + 120.0f;
      else              h = 60.0f * (r - g) / chroma + 240.0f;
      int bin = ((int)(h / (360.0f / BINS))) % BINS;
      weight[bin] += chroma; sumR[bin] += r * chroma; sumG[bin] += g * chroma; sumB[bin] += b * chroma;
      ++colorful;
    }
  }

  float h = 0, s = 0;
  if (colorful * 100 >= samples * 6) {
    int best = 0; uint32_t bestScore = 0;
    for (int i = 0; i < BINS; ++i) {
      uint32_t score = weight[i] * 2 + weight[(i + 1) % BINS] + weight[(i + BINS - 1) % BINS];
      if (score > bestScore) { bestScore = score; best = i; }
    }
    float r = (float)sumR[best] / weight[best], g = (float)sumG[best] / weight[best], b = (float)sumB[best] / weight[best];
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float c = mx - mn;
    if (c > 1) {
      if (mx == r)      h = 60.0f * (g - b) / c + (g < b ? 360.0f : 0.0f);
      else if (mx == g) h = 60.0f * (b - r) / c + 120.0f;
      else              h = 60.0f * (r - g) / c + 240.0f;
      float l = (mx + mn) / 510.0f;
      float d = 1 - (l * 2 > 1 ? l * 2 - 1 : 1 - l * 2);
      s = d > 0.01f ? (c / 255.0f) / d : 0;
      if (s > 1) s = 1;
    }
  }

  return paletteFromHS(h, s);
}

// ---------------------------------------------------------------- helpers
static void uiBegin(lgfx::LovyanGFX* screen) {
  gfx = screen;
  barSprite = new LGFX_Sprite(screen);
  barSprite->setColorDepth(16);
  barSprite->createSprite(SCREEN_W, BAR_AREA_H);
}

static Zone zoneAt(int x, int y) {
  if (y >= BTN_Y - 4) {
    if (x < BTN_X[1] - 4) return ZONE_PREV;
    if (x < BTN_X[2] - 4) return ZONE_PLAY;
    return ZONE_NEXT;
  }
  if (y >= BAR_AREA_Y - 6 && y < BTN_Y - 4) return ZONE_BAR;
  return ZONE_NONE;
}

static uint32_t barMsFromX(int x, uint32_t durMs) {
  if (x < BAR_X) x = BAR_X;
  if (x > BAR_X + BAR_W) x = BAR_X + BAR_W;
  return (uint32_t)((uint64_t)durMs * (uint32_t)(x - BAR_X) / BAR_W);
}

static void fmtTime(char* out, size_t n, uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s >= 3600) snprintf(out, n, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
  else           snprintf(out, n, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

// Word-wraps text into at most maxLines lines; the last line gets "..." if it overflows.
// Returns the y position just below the text.
static int drawWrapped(const char* text, int x, int y, int maxW, int maxLines, int lineH) {
  char line[132];
  const char* p = text;
  for (int ln = 0; ln < maxLines; ++ln) {
    while (*p == ' ') ++p;
    size_t remaining = strlen(p);
    if (remaining == 0) break;
    if (remaining > 127) remaining = 127;

    size_t fit = 0, lastBreak = 0;
    for (size_t i = 1; i <= remaining; ++i) {
      memcpy(line, p, i); line[i] = 0;
      if (gfx->textWidth(line) > maxW) break;
      fit = i;
      if (p[i] == ' ' || p[i] == 0) lastBreak = i;
    }

    size_t take;
    bool ellipsis = false;
    if (fit == remaining) {
      take = remaining;
    } else if (ln == maxLines - 1) {
      ellipsis = true;
      take = fit;
      while (take > 0) {
        memcpy(line, p, take); strcpy(line + take, "...");
        if (gfx->textWidth(line) <= maxW) break;
        --take;
      }
      while (take > 0 && p[take - 1] == ' ') --take;
    } else {
      take = lastBreak ? lastBreak : (fit ? fit : 1);
    }
    memcpy(line, p, take); line[take] = 0;
    if (ellipsis) strcpy(line + take, "...");
    gfx->drawString(line, x, y);
    y += lineH;
    p += take;
  }
  return y;
}

static void drawHazardBand(int y, int h) {
  gfx->fillRect(0, y, SCREEN_W, h, pal.bg);
  gfx->setClipRect(0, y, SCREEN_W, h);
  for (int x = -h; x < SCREEN_W + h; x += 2 * h) {
    gfx->fillTriangle(x, y + h, x + h, y, x + 2 * h, y, pal.accent);
    gfx->fillTriangle(x, y + h, x + h, y + h, x + 2 * h, y, pal.accent);
  }
  gfx->clearClipRect();
}

// Small grab bar at the top edge: a reminder that swiping down opens the menu.
static void drawPullHandle(uint32_t col = pal.dim) {
  gfx->fillRect(SCREEN_W / 2 - 22, 1, 44, 3, col);
}

// ---------------------------------------------------------------- full-screen messages
static void drawMessageScreen(const char* big, const char* line1, const char* line2, const char* hint = nullptr) {
  gfx->startWrite();
  gfx->fillScreen(pal.bg);
  drawHazardBand(0, 12);
  drawHazardBand(SCREEN_H - 12, 12);
  gfx->drawRect(8, 22, SCREEN_W - 16, SCREEN_H - 44, pal.dim);
  gfx->setTextDatum(lgfx::textdatum_t::top_center);
  gfx->setFont(FONT_BIG);
  gfx->setTextColor(pal.accent, pal.bg);
  gfx->drawString(big, SCREEN_W / 2, 82);
  gfx->setFont(FONT_BODY);
  gfx->setTextColor(pal.accent, pal.bg);
  if (line1) gfx->drawString(line1, SCREEN_W / 2, 122);
  if (line2) gfx->drawString(line2, SCREEN_W / 2, 142);
  if (hint) {
    gfx->setFont(FONT_SMALL);
    gfx->setTextColor(pal.dim, pal.bg);
    gfx->drawString(hint, SCREEN_W / 2, 198);
  }
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  gfx->endWrite();
}

static const char* const MENU_HINT = "SWIPE DOWN FOR MENU";
static void drawNoLink()  { drawMessageScreen("NO SIGNAL", "Waiting for the computer.", "Start the companion app.", MENU_HINT); }
static void drawStandby() { drawMessageScreen("STANDBY", "Nothing is playing.", nullptr, MENU_HINT); }


// ---------------------------------------------------------------- now-playing pieces
static void drawArt(const UiState& s) {
  gfx->startWrite();
  gfx->drawRect(ART_X - 2, ART_Y - 2, ART_SIZE + 4, ART_SIZE + 4, musicPal.accent);
  gfx->drawRect(ART_X - 1, ART_Y - 1, ART_SIZE + 2, ART_SIZE + 2, musicPal.bg);
  if (s.artValid && s.art) {
    gfx->pushImage(ART_X, ART_Y, ART_SIZE, ART_SIZE, reinterpret_cast<const lgfx::swap565_t*>(s.art));
  } else {
    gfx->fillRect(ART_X, ART_Y, ART_SIZE, ART_SIZE, musicPal.rust);
    gfx->setClipRect(ART_X, ART_Y, ART_SIZE, ART_SIZE);
    for (int d = -ART_SIZE; d < ART_SIZE; d += 14)
      gfx->drawLine(ART_X + d, ART_Y + ART_SIZE, ART_X + d + ART_SIZE, ART_Y, musicPal.dim);
    gfx->clearClipRect();
  }
  gfx->endWrite();
}

static void drawStatusLabel(const UiState& s) {
  gfx->startWrite();
  gfx->fillRect(TEXT_X, LABEL_Y, TEXT_W, 9, musicPal.bg);
  gfx->setFont(FONT_SMALL);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);
  if (s.playing) {
    gfx->fillRect(TEXT_X, LABEL_Y, 7, 7, musicPal.accent);
    gfx->setTextColor(musicPal.accent, musicPal.bg);
    gfx->drawString("NOW PLAYING", TEXT_X + 12, LABEL_Y);
  } else {
    gfx->drawRect(TEXT_X, LABEL_Y, 7, 7, musicPal.dim);
    gfx->setTextColor(musicPal.dim, musicPal.bg);
    gfx->drawString("PAUSED", TEXT_X + 12, LABEL_Y);
  }
  gfx->drawFastHLine(TEXT_X, LABEL_Y + 12, TEXT_W, musicPal.dim);
  gfx->endWrite();
}

static void drawTrackText(const UiState& s) {
  gfx->startWrite();
  gfx->fillRect(TEXT_X, TITLE_Y - 2, SCREEN_W - TEXT_X, BAR_AREA_Y - (TITLE_Y - 2), musicPal.bg);
  gfx->setTextDatum(lgfx::textdatum_t::top_left);

  gfx->setFont(FONT_TITLE);
  gfx->setTextColor(musicPal.cream, musicPal.bg);
  int y = drawWrapped(s.title[0] ? s.title : "Unknown track", TEXT_X, TITLE_Y, TEXT_W, 3, TITLE_LINE_H);

  y += 5;
  gfx->setFont(FONT_BODY);
  gfx->setTextColor(musicPal.accent, musicPal.bg);
  if (s.artist[0]) y = drawWrapped(s.artist, TEXT_X, y, TEXT_W, 2, BODY_LINE_H);

  y += 4;
  gfx->setFont(FONT_SMALL);
  gfx->setTextColor(musicPal.dim, musicPal.bg);
  if (s.album[0] && y + 8 <= BAR_AREA_Y - 2) drawWrapped(s.album, TEXT_X, y, TEXT_W, 1, 10);
  gfx->endWrite();
}

// Gauge + elapsed / total time. Drawn into an off-screen strip first so it never flickers.
static void drawBar(const UiState& s) {
  LGFX_Sprite& b = *barSprite;
  b.fillScreen(musicPal.bg);

  bool known = s.hasPos && s.durMs > 0;
  uint32_t pos = s.posMs > s.durMs ? s.durMs : s.posMs;
  int fillPx = known ? (int)((uint64_t)BAR_W * pos / s.durMs) : 0;

  for (int x = 0; x + SEG_W <= BAR_W; x += SEG_PITCH)
    b.fillRect(BAR_X + x, BAR_Y, SEG_W, BAR_H, (known && x + SEG_W / 2 < fillPx) ? musicPal.accent : musicPal.rust);

  if (known) {
    int hx = BAR_X + fillPx - 4;
    if (hx < BAR_X - 2) hx = BAR_X - 2;
    if (hx > BAR_X + BAR_W - 6) hx = BAR_X + BAR_W - 6;
    b.fillRect(hx - 1, BAR_Y - 6, 10, BAR_H + 12, musicPal.bg);
    b.fillRect(hx, BAR_Y - 5, 8, BAR_H + 10, musicPal.cream);
  }

  char t[16];
  b.setFont(FONT_BODY);
  b.setTextColor(musicPal.accent, musicPal.bg);
  b.setTextDatum(lgfx::textdatum_t::top_left);
  if (known) fmtTime(t, sizeof t, pos); else strcpy(t, "--:--");
  b.drawString(t, BAR_X, BAR_Y + BAR_H + 8);
  b.setTextColor(musicPal.dim, musicPal.bg);
  b.setTextDatum(lgfx::textdatum_t::top_right);
  if (known) fmtTime(t, sizeof t, s.durMs); else strcpy(t, "--:--");
  b.drawString(t, BAR_X + BAR_W, BAR_Y + BAR_H + 8);

  b.pushSprite(0, BAR_AREA_Y);
}

static void drawButton(int which, const UiState& s) {
  int x = BTN_X[which], y = BTN_Y;
  bool down = (s.pressed == which);
  uint32_t face, ink;
  if (which == ZONE_PLAY) { face = down ? musicPal.cream : musicPal.accent; ink = musicPal.bg; }
  else                    { face = down ? musicPal.accent : musicPal.bg;    ink = down ? musicPal.bg : musicPal.accent; }

  gfx->startWrite();
  gfx->fillRect(x, y, BTN_W, BTN_H, face);
  if (which != ZONE_PLAY) {
    gfx->drawRect(x, y, BTN_W, BTN_H, musicPal.accent);
    gfx->drawRect(x + 1, y + 1, BTN_W - 2, BTN_H - 2, musicPal.accent);
  }
  int cx = x + BTN_W / 2, cy = y + BTN_H / 2;
  if (which == ZONE_PLAY) {
    if (s.playing) {
      gfx->fillRect(cx - 9, cy - 10, 6, 21, ink);
      gfx->fillRect(cx + 3, cy - 10, 6, 21, ink);
    } else {
      gfx->fillTriangle(cx - 7, cy - 10, cx - 7, cy + 10, cx + 10, cy, ink);
    }
  } else if (which == ZONE_NEXT) {
    gfx->fillTriangle(cx - 12, cy - 9, cx - 12, cy + 9, cx - 1, cy, ink);
    gfx->fillTriangle(cx - 1, cy - 9, cx - 1, cy + 9, cx + 10, cy, ink);
    gfx->fillRect(cx + 10, cy - 9, 3, 19, ink);
  } else {
    gfx->fillTriangle(cx + 12, cy - 9, cx + 12, cy + 9, cx + 1, cy, ink);
    gfx->fillTriangle(cx + 1, cy - 9, cx + 1, cy + 9, cx - 10, cy, ink);
    gfx->fillRect(cx - 13, cy - 9, 3, 19, ink);
  }
  gfx->endWrite();
}

static void drawButtons(const UiState& s) {
  for (int i = 0; i < 3; ++i) drawButton(i, s);
}

static void drawPlayingScreen(const UiState& s) {
  gfx->fillScreen(musicPal.bg);
  drawPullHandle(musicPal.dim);
  drawArt(s);
  drawStatusLabel(s);
  drawTrackText(s);
  drawBar(s);
  drawButtons(s);
}
