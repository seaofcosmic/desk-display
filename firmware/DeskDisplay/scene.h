// scene.h - the full-screen animated sky of the Weather app in the Full color look.
// Sunny: blue sky and a turning sun. Rain: drifting clouds and falling pixel rain.
// Thunder: darker clouds, heavier rain and lightning. Also cloudy, fog, snow and clear nights.
//
// The sky is drawn in a 16-color picture the size of the screen and copied to the screen about
// 11 times a second. Each name in SceneColor is a slot in that 16-color palette; sceneColors()
// fills the slots for the current weather, so recoloring a scene only means changing that function.
//
// sceneBlurToScreen() paints a blurred, darkened copy of the sky, used behind the details.
#pragma once
#include "ui.h"
#include <math.h>
#include <stdlib.h>

static const int SCENE_W = SCREEN_W, SCENE_H = SCREEN_H;
static const uint32_t SCENE_FRAME_MS = 90;

enum SceneColor {
  SC_SKY_TOP, SC_SKY_MID, SC_SKY_LOW, SC_CLOUD, SC_CLOUD_SHADE, SC_DROP, SC_SUN, SC_RAY,
  SC_BOLT, SC_TEXT, SC_TEXT_SOFT, SC_SHADOW, SC_STAR, SC_FOG, SC_COUNT
};

// What to show. kind uses the same numbers as the weather icons:
// 0 clear, 1 partly cloudy, 2 cloudy, 3 fog, 4 rain, 5 snow, 6 storm.
struct SceneInfo {
  int kind = 0;
  bool night = false;
  const char* city = "";
  const char* temp = "";
  const char* unit = "F";
  const char* cond = "";
  const char* hi = "";
  const char* lo = "";
};

static const int SCENE_DROPS = 150, SCENE_CLOUDS = 6, SCENE_STARS = 48, SCENE_BOLT_POINTS = 9;
struct SceneState {
  int kind = -1;
  bool night = false;
  uint32_t frame = 0;
  uint32_t rng = 12345;
  float cloudX[SCENE_CLOUDS];
  int16_t dropX[SCENE_DROPS], dropY[SCENE_DROPS];
  uint8_t dropSpeed[SCENE_DROPS];
  int flash = 0;            // frames of bright sky left
  int boltFrames = 0;       // frames the bolt stays visible
  int nextBolt = 30;        // frames until the next lightning
  int16_t boltX[SCENE_BOLT_POINTS], boltY[SCENE_BOLT_POINTS];
};

static LGFX_Sprite* sceneSprite = nullptr;
static uint32_t sceneSky[3];
static uint32_t scenePalette[16];      // what each slot is currently set to, as 0xRRGGBB

static bool sceneReady = false;        // false if there was not enough memory for the sky picture

static bool sceneBegin(lgfx::LovyanGFX* screen) {
  sceneSprite = new LGFX_Sprite(screen);
  sceneSprite->setColorDepth(4);
  sceneReady = sceneSprite->createSprite(SCENE_W, SCENE_H) != nullptr;
  if (sceneReady) sceneSprite->createPalette();
  return sceneReady;
}

static void sceneSet(int slot, uint32_t rgb) {
  scenePalette[slot] = rgb;
  sceneSprite->setPaletteColor(slot, rgb);
}

static uint32_t sceneRandom(SceneState& st, uint32_t below) {
  st.rng = st.rng * 1664525u + 1013904223u;
  return (st.rng >> 8) % below;
}

static void sceneSetSky(uint32_t top, uint32_t mid, uint32_t low) {
  sceneSet(SC_SKY_TOP, top);
  sceneSet(SC_SKY_MID, mid);
  sceneSet(SC_SKY_LOW, low);
}

// ---------------------------------------------------------------- colors for each kind of weather
static void sceneColors(int kind, bool night) {
  uint32_t cloud = 0xF2F5F8, shade = 0xC2CCD9, drop = 0x9CC4F5;
  switch (kind) {
    case 0: case 1: sceneSky[0] = 0x1E6FD9; sceneSky[1] = 0x3F8FE8; sceneSky[2] = 0x6FB4F2; break;   // sunny blue
    case 2:         sceneSky[0] = 0x55667A; sceneSky[1] = 0x697A8E; sceneSky[2] = 0x8190A3; cloud = 0xDDE3EA; shade = 0xAEB8C6; break;
    case 3:         sceneSky[0] = 0x666B73; sceneSky[1] = 0x7B8088; sceneSky[2] = 0x92979E; break;
    case 4:         sceneSky[0] = 0x1F2B3D; sceneSky[1] = 0x2A3A52; sceneSky[2] = 0x354A66; cloud = 0x7C8798; shade = 0x5A6577; break;
    case 5:         sceneSky[0] = 0x2E3A4D; sceneSky[1] = 0x3D4B61; sceneSky[2] = 0x52627A; cloud = 0xA9B4C4; shade = 0x7F8B9E; drop = 0xFFFFFF; break;
    default:        sceneSky[0] = 0x15131F; sceneSky[1] = 0x211C33; sceneSky[2] = 0x2D2647; cloud = 0x545170; shade = 0x38364F; break;
  }
  if (night && kind <= 2) {
    sceneSky[0] = 0x070B1F; sceneSky[1] = 0x0D1433; sceneSky[2] = 0x152047;
    cloud = 0x39425A; shade = 0x2A3147;
  }
  sceneSetSky(sceneSky[0], sceneSky[1], sceneSky[2]);
  sceneSet(SC_CLOUD, cloud);
  sceneSet(SC_CLOUD_SHADE, shade);
  sceneSet(SC_DROP, drop);
  sceneSet(SC_SUN, night ? 0xE8E6D8u : 0xFFD23Fu);      // the moon uses the sun's slot at night
  sceneSet(SC_RAY, 0xFFB000u);
  sceneSet(SC_BOLT, 0xFFF7B0u);
  sceneSet(SC_TEXT, 0xFFFFFFu);
  sceneSet(SC_TEXT_SOFT, 0xE3ECF7u);
  sceneSet(SC_SHADOW, 0x0A0E18u);
  sceneSet(SC_STAR, 0xFFFFFFu);
  sceneSet(SC_FOG, 0xB4BAC2u);
}

// The color family that goes with this sky (used for the details view): its hue and strength.
static void sceneHue(int kind, bool night, float& hue, float& sat) {
  if (night && kind <= 2) { hue = 230; sat = 0.60f; return; }
  switch (kind) {
    case 0: case 1: hue = 208; sat = 0.75f; break;
    case 2:         hue = 212; sat = 0.25f; break;
    case 3:         hue = 215; sat = 0.08f; break;
    case 4:         hue = 215; sat = 0.40f; break;
    case 5:         hue = 205; sat = 0.22f; break;
    default:        hue = 262; sat = 0.40f; break;
  }
}

// ---------------------------------------------------------------- starting and moving a scene
static int sceneDropCount(int kind) { return kind == 6 ? SCENE_DROPS : (kind == 4 ? 105 : (kind == 5 ? 80 : 0)); }
static const int SCENE_RAIN_TOP = 44;      // rain and snow appear from under the clouds

static void sceneStart(SceneState& st, int kind, bool night) {
  st.kind = kind;
  st.night = night;
  st.frame = 0;
  st.flash = 0; st.boltFrames = 0; st.nextBolt = 20;
  for (int i = 0; i < SCENE_CLOUDS; ++i) st.cloudX[i] = (float)(i * 78) - 60.0f + (float)sceneRandom(st, 40);
  for (int i = 0; i < SCENE_DROPS; ++i) {
    st.dropX[i] = (int16_t)sceneRandom(st, SCENE_W + 60);
    st.dropY[i] = (int16_t)(SCENE_RAIN_TOP + sceneRandom(st, SCENE_H - SCENE_RAIN_TOP));
    st.dropSpeed[i] = (uint8_t)(kind == 5 ? 1 + sceneRandom(st, 2) : 8 + sceneRandom(st, 5));
  }
  sceneColors(kind, night);
}

static void sceneStep(SceneState& st) {
  ++st.frame;
  static const float SPEED[SCENE_CLOUDS] = { 0.35f, 0.55f, 0.25f, 0.45f, 0.30f, 0.50f };
  for (int i = 0; i < SCENE_CLOUDS; ++i) {
    st.cloudX[i] += SPEED[i] * (st.kind >= 4 ? 1.6f : 1.0f);
    if (st.cloudX[i] > SCENE_W + 10) st.cloudX[i] = -170;
  }
  int count = sceneDropCount(st.kind);
  if (st.kind == 4 || st.kind == 6) {                       // rain: fast, slanting left
    for (int i = 0; i < count; ++i) {
      st.dropY[i] += st.dropSpeed[i];
      st.dropX[i] -= 1;
      if (st.dropY[i] >= SCENE_H) { st.dropY[i] = (int16_t)(SCENE_RAIN_TOP + sceneRandom(st, 14)); st.dropX[i] = (int16_t)sceneRandom(st, SCENE_W + 60); }
    }
  } else if (st.kind == 5) {                                // snow: slow, swaying
    for (int i = 0; i < count; ++i) {
      if (st.frame % 2 == 0) st.dropY[i] += st.dropSpeed[i];
      if (st.frame % 6 == 0) st.dropX[i] += ((st.frame / 6 + i) % 4 < 2) ? 1 : -1;
      if (st.dropY[i] >= SCENE_H) { st.dropY[i] = (int16_t)(SCENE_RAIN_TOP + sceneRandom(st, 14)); st.dropX[i] = (int16_t)sceneRandom(st, SCENE_W); }
    }
  }
  if (st.kind == 6) {                                       // lightning
    if (st.flash > 0) --st.flash;
    if (st.boltFrames > 0) --st.boltFrames;
    if (--st.nextBolt <= 0) {
      st.nextBolt = 28 + (int)sceneRandom(st, 45);
      st.flash = 2;
      st.boltFrames = 5;
      int x = 60 + (int)sceneRandom(st, 200), y = 48;
      for (int i = 0; i < SCENE_BOLT_POINTS; ++i) {
        st.boltX[i] = (int16_t)x; st.boltY[i] = (int16_t)y;
        x += (int)sceneRandom(st, 34) - 17;
        y += 15 + (int)sceneRandom(st, 9);
      }
    }
  }
}

// ---------------------------------------------------------------- drawing one frame
// A puffy cloud, 22 x 12 cells. u = pixels per cell.
static void sceneCloud(LGFX_Sprite& s, int x, int y, int u) {
  s.fillRoundRect(x + u, y + 6 * u, 20 * u, 6 * u, 3 * u, SC_CLOUD_SHADE);       // the shaded underside
  s.fillRoundRect(x, y + 5 * u, 22 * u, 6 * u, 3 * u, SC_CLOUD);                 // the body
  s.fillCircle(x + 13 * u / 2, y + 11 * u / 2, 7 * u / 2, SC_CLOUD);             // left puff
  s.fillCircle(x + 12 * u, y + 4 * u, 4 * u, SC_CLOUD);                          // tall middle puff
  s.fillCircle(x + 17 * u, y + 6 * u, 3 * u, SC_CLOUD);                          // right puff
}

static void sceneShadowText(LGFX_Sprite& s, const char* text, int x, int y, int color, int offset = 1) {
  s.setTextColor(SC_SHADOW); s.drawString(text, x + offset, y + offset);
  s.setTextColor(color);     s.drawString(text, x, y);
}

// words = false draws the sky alone. show = false draws it without copying it to the screen.
static void sceneDraw(SceneState& st, const SceneInfo& info, bool words = true, bool show = true) {
  LGFX_Sprite& s = *sceneSprite;
  int kind = st.kind;
  bool darkSky = st.night && kind <= 2;

  // sky: three bands with a dotted edge between them
  if (kind == 6) {
    if (st.flash > 0) sceneSetSky(0x8F8AB8, 0xA29DC6, 0xB7B3D6);
    else              sceneSetSky(sceneSky[0], sceneSky[1], sceneSky[2]);
  }
  int b1 = SCENE_H * 34 / 100, b2 = SCENE_H * 68 / 100;
  s.fillRect(0, 0, SCENE_W, b1, SC_SKY_TOP);
  s.fillRect(0, b1, SCENE_W, b2 - b1, SC_SKY_MID);
  s.fillRect(0, b2, SCENE_W, SCENE_H - b2, SC_SKY_LOW);
  for (int x = 0; x < SCENE_W; x += 8) {
    s.fillRect(x, b1, 4, 2, SC_SKY_TOP);
    s.fillRect(x + 4, b2, 4, 2, SC_SKY_MID);
  }

  if (darkSky) {                                            // stars that blink
    uint32_t r = 777;
    for (int i = 0; i < SCENE_STARS; ++i) {
      r = r * 1664525u + 1013904223u; int x = (r >> 8) % SCENE_W;
      r = r * 1664525u + 1013904223u; int y = (r >> 8) % (SCENE_H - 60);
      if (((st.frame / 7) + i) % 6 != 0) s.fillRect(x, y, 2, 2, SC_STAR);
    }
  }

  if (kind <= 1) {                                          // sun, or the moon at night
    int cx = 250, cy = 62;
    if (st.night) {
      s.fillCircle(cx, cy, 24, SC_SUN);
      s.fillCircle(cx + 11, cy - 8, 20, SC_SKY_TOP);
    } else {
      float a0 = st.frame * 0.03f;
      int reach = 43 + ((st.frame / 8) % 2) * 2;
      for (int i = 0; i < 8; ++i) {
        float a = a0 + i * 0.7853982f;
        int rx = cx + (int)lroundf(cosf(a) * reach), ry = cy + (int)lroundf(sinf(a) * reach);
        s.fillRect(rx - 4, ry - 4, 9, 9, SC_RAY);
      }
      s.fillCircle(cx, cy, 30, SC_SUN);
    }
  }

  if (kind == 3) {                                          // fog: long bands sliding past each other
    for (int i = 0; i < 10; ++i) {
      int w = 150 + (i % 5) * 24;
      int x = (int)((st.frame * (i % 2 ? 0.5f : -0.4f)) + i * 67) % (SCENE_W + w);
      if (x < 0) x += SCENE_W + w;
      s.fillRect(x - w, 22 + i * 21, w, 7, SC_FOG);
    }
  }

  // Clouds stay in the top part of the sky, clear of the big temperature.
  if (kind == 1) {                                          // a few clouds
    sceneCloud(s, (int)st.cloudX[0], 50, 3);
    sceneCloud(s, (int)st.cloudX[2] + 40, 18, 3);
    sceneCloud(s, (int)st.cloudX[4] - 60, 64, 2);
  } else if (kind == 2) {                                   // overcast
    sceneCloud(s, (int)st.cloudX[0], 22, 5);
    sceneCloud(s, (int)st.cloudX[1] + 20, 46, 3);
    sceneCloud(s, (int)st.cloudX[2] + 60, 4, 4);
    sceneCloud(s, (int)st.cloudX[3] + 10, 52, 3);
    sceneCloud(s, (int)st.cloudX[4] - 30, 30, 4);
    sceneCloud(s, (int)st.cloudX[5] + 30, 10, 4);
  } else if (kind >= 4) {                                   // heavy clouds along the top
    sceneCloud(s, (int)st.cloudX[0], -18, 5);
    sceneCloud(s, (int)st.cloudX[1] + 20, -24, 5);
    sceneCloud(s, (int)st.cloudX[2] + 60, -6, 4);
    sceneCloud(s, (int)st.cloudX[3] + 10, -20, 5);
    sceneCloud(s, (int)st.cloudX[4] - 20, -10, 4);
    sceneCloud(s, (int)st.cloudX[5] + 40, -22, 5);
  }

  int count = sceneDropCount(kind);
  if (kind == 4 || kind == 6) {
    for (int i = 0; i < count; ++i) s.fillRect(st.dropX[i], st.dropY[i], 2, 9, SC_DROP);
  } else if (kind == 5) {
    for (int i = 0; i < count; ++i) s.fillRect(st.dropX[i], st.dropY[i], 3, 3, SC_DROP);
  }

  if (kind == 6 && st.boltFrames > 0) {
    for (int i = 0; i + 1 < SCENE_BOLT_POINTS; ++i)
      for (int t = -1; t <= 1; ++t)
        s.drawLine(st.boltX[i] + t, st.boltY[i], st.boltX[i + 1] + t, st.boltY[i + 1], SC_BOLT);
  }

  if (!words) { if (show) s.pushSprite(0, 0); return; }

  // the words and numbers, each with a dark edge so they read on any sky
  s.fillRect(SCENE_W / 2 - 22, 1, 44, 3, SC_TEXT_SOFT);                    // the menu grab bar
  s.setTextDatum(lgfx::textdatum_t::top_left);
  s.setFont(FONT_SMALL);
  sceneShadowText(s, info.city, 10, 12, SC_TEXT);

  s.setFont(&fonts::Font7);
  s.setTextSize(2);
  int tx = 12, ty = 92;
  sceneShadowText(s, info.temp, tx, ty, SC_TEXT, 3);
  int ux = tx + s.textWidth(info.temp) + 12;
  s.setTextSize(1);
  for (int r = 5; r <= 8; ++r) s.drawCircle(ux + 2, ty + 10, r, SC_SHADOW);
  for (int r = 5; r <= 8; ++r) s.drawCircle(ux, ty + 8, r, SC_TEXT);
  s.setFont(FONT_BIG);
  sceneShadowText(s, info.unit, ux + 12, ty + 2, SC_TEXT, 2);

  s.setFont(FONT_BIG);
  sceneShadowText(s, info.cond, 12, 196, SC_TEXT, 2);
  char hl[32];
  snprintf(hl, sizeof hl, "H %s   L %s", info.hi, info.lo);
  s.setFont(FONT_BODY);
  s.setTextDatum(lgfx::textdatum_t::top_right);
  sceneShadowText(s, hl, SCENE_W - 10, 172, SC_TEXT_SOFT);

  // hint: swipe up for the details
  s.setFont(FONT_SMALL);
  sceneShadowText(s, "SWIPE UP FOR DETAILS", SCENE_W - 10, 226, SC_TEXT_SOFT);
  s.setTextDatum(lgfx::textdatum_t::top_left);

  if (show) s.pushSprite(0, 0);
}

// ---------------------------------------------------------------- the blurred sky
// Paints the current sky onto the screen out of focus and darkened, so text can sit on top.
// It shrinks the picture to a quarter size, softens that, and stretches it back smoothly.
// keepPercent: how much brightness to keep (40 = fairly dark). Returns false if memory ran out.
static bool sceneBlurToScreen(lgfx::LovyanGFX* screen, int keepPercent) {
  static const int SW = SCENE_W / 4, SH = SCENE_H / 4;
  uint8_t* buf = (uint8_t*)malloc((size_t)SW * SH * 3 * 2);
  if (!buf) return false;
  uint8_t* a = buf;
  uint8_t* b = buf + SW * SH * 3;

  // shrink: each small pixel is the average of four samples from its 4 x 4 block
  for (int y = 0; y < SH; ++y) {
    for (int x = 0; x < SW; ++x) {
      int r = 0, g = 0, bl = 0;
      for (int k = 0; k < 4; ++k) {
        uint32_t c = scenePalette[sceneSprite->readPixelValue(x * 4 + (k & 1) * 2 + 1, y * 4 + (k >> 1) * 2 + 1) & 15];
        r += (c >> 16) & 255; g += (c >> 8) & 255; bl += c & 255;
      }
      uint8_t* p = a + (y * SW + x) * 3;
      p[0] = (uint8_t)(r / 4); p[1] = (uint8_t)(g / 4); p[2] = (uint8_t)(bl / 4);
    }
  }

  // soften: a small blur sideways then up-and-down, done three times
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < SH; ++y)
      for (int x = 0; x < SW; ++x) {
        int xl = x > 0 ? x - 1 : x, xr = x < SW - 1 ? x + 1 : x;
        const uint8_t* l = a + (y * SW + xl) * 3; const uint8_t* m = a + (y * SW + x) * 3; const uint8_t* r = a + (y * SW + xr) * 3;
        uint8_t* o = b + (y * SW + x) * 3;
        for (int c = 0; c < 3; ++c) o[c] = (uint8_t)((l[c] + 2 * m[c] + r[c]) / 4);
      }
    for (int y = 0; y < SH; ++y) {
      int yu = y > 0 ? y - 1 : y, yd = y < SH - 1 ? y + 1 : y;
      for (int x = 0; x < SW; ++x) {
        const uint8_t* u = b + (yu * SW + x) * 3; const uint8_t* m = b + (y * SW + x) * 3; const uint8_t* d = b + (yd * SW + x) * 3;
        uint8_t* o = a + (y * SW + x) * 3;
        for (int c = 0; c < 3; ++c) o[c] = (uint8_t)((u[c] + 2 * m[c] + d[c]) / 4);
      }
    }
  }

  // stretch back to full size, blending between neighbors, one screen row at a time
  static uint8_t row[SCENE_W * 2];
  int keep = keepPercent * 256 / 100;
  screen->startWrite();
  for (int y = SCENE_H - 1; y >= 0; --y) {                  // bottom row first, so it rises into view
    int v = y * (SH - 1) * 256 / (SCENE_H - 1);
    int y0 = v >> 8, wy = v & 255, y1 = y0 < SH - 1 ? y0 + 1 : y0;
    for (int x = 0; x < SCENE_W; ++x) {
      int u = x * (SW - 1) * 256 / (SCENE_W - 1);
      int x0 = u >> 8, wx = u & 255, x1 = x0 < SW - 1 ? x0 + 1 : x0;
      const uint8_t* p00 = a + (y0 * SW + x0) * 3; const uint8_t* p01 = a + (y0 * SW + x1) * 3;
      const uint8_t* p10 = a + (y1 * SW + x0) * 3; const uint8_t* p11 = a + (y1 * SW + x1) * 3;
      int rgb[3];
      for (int c = 0; c < 3; ++c) {
        int top = p00[c] * (256 - wx) + p01[c] * wx;
        int bot = p10[c] * (256 - wx) + p11[c] * wx;
        rgb[c] = (((top * (256 - wy) + bot * wy) >> 16) * keep) >> 8;
      }
      uint16_t px = (uint16_t)(((rgb[0] & 0xF8) << 8) | ((rgb[1] & 0xFC) << 3) | (rgb[2] >> 3));
      row[x * 2] = (uint8_t)(px >> 8);
      row[x * 2 + 1] = (uint8_t)(px & 0xFF);
    }
    screen->pushImage(0, y, SCENE_W, 1, reinterpret_cast<const lgfx::swap565_t*>(row));
  }
  screen->endWrite();
  free(buf);
  return true;
}
