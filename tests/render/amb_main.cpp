#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
#include <vector>
static LGFX_Sprite canvas;
static void dump(const char* name) {
  char p[80]; snprintf(p, sizeof p, "%s.ppm", name);
  FILE* f = fopen(p, "wb");
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; ++y) for (int x = 0; x < SCREEN_W; ++x) {
    uint16_t c = canvas.readPixel(x, y);
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static uint8_t art[ART_BYTES];
static bool loadArt(const char* name) {
  char p[120]; snprintf(p, sizeof p, "../preview/%s.rgb565", name);
  FILE* f = fopen(p, "rb"); if (!f) return false;
  size_t n = fread(art, 1, ART_BYTES, f); fclose(f); return n == ART_BYTES;
}
static uint32_t at(int h, int m, int s = 0) { return 1794787200u /* some midnight */ / 86400 * 86400 + h * 3600 + m * 60 + s; }

int main() {
  canvas.setColorDepth(16); canvas.createSprite(SCREEN_W, SCREEN_H); uiBegin(&canvas); sceneBegin(&canvas);
  Weather w; ClockState c; c.known = true;
  // sunrise 6:27 (387), sunset 16:52 (1012)
  const char* CLEAR  = "OK\nCULVER CITY\n18\nC\nCLEAR\n0\n22\n13\n17\n6 KM/H\n58%\n6:30 AM\nD|387|1012\nTODAY|0|22|13\nTUE|4|18|11\nWED|0|24|13\n";
  const char* PARTLY = "OK\nCULVER CITY\n20\nC\nPARTLY CLOUDY\n1\n22\n13\n19\n13 KM/H\n61%\n4:50 PM\nD|387|1012\nTODAY|1|22|13\nTUE|4|18|11\nWED|0|24|13\n";
  const char* CLOUDY = "OK\nSEATTLE\n11\nC\nCLOUDY\n2\n12\n7\n9\n8 KM/H\n62%\n4:55 PM\nD|387|1012\nTODAY|2|12|7\nSAT|2|11|6\nSUN|4|10|6\n";
  const char* RAIN   = "OK\nCULVER CITY\n14\nC\nRAIN\n4\n16\n11\n13\n19 KM/H\n88%\n3:05 PM\nD|387|1012\nTODAY|4|16|11\nTUE|4|16|11\nWED|2|19|12\n";
  const char* OLD    = "OK\nCULVER CITY\n61\nF\nCLEAR\n0\n72\n55\n60\n4 MPH\n70%\n11:20 PM\nN\nTODAY|0|72|55\nTUE|4|64|52\nWED|0|75|56\n";

  // ---- weather skies at sunrise and sunset
  fullColorMode = true;
  struct { const char* name; const char* data; int h, m; } wx[] = {
    { "w1_dawn_clear", CLEAR, 6, 30 }, { "w2_dusk_partly", PARTLY, 16, 50 }, { "w3_dusk_cloudy", CLOUDY, 17, 5 },
    { "w4_day_clear", CLEAR, 12, 0 }, { "w5_night_clear", CLEAR, 22, 0 }, { "w6_dusk_rain", RAIN, 16, 50 },
    { "w7_old_night_flag", OLD, 12, 0 },
  };
  for (auto& k : wx) {
    parseWeather(k.data, w); c.epoch = at(k.h, k.m);
    drawWeatherScreen(w, c);
    for (int i = 0; i < 40; ++i) drawWeatherSky(w, c, true);
    dump(k.name);
  }
  parseWeather(PARTLY, w); c.epoch = at(16, 50);
  drawWeatherDetails(w, c); dump("w8_dusk_details");

  // ---- the clock on the sky
  struct { const char* name; const char* data; int h, m, s; bool h24; } ck[] = {
    { "c1_dawn", CLEAR, 6, 35, 7, false }, { "c2_morning", CLEAR, 9, 10, 32, false }, { "c3_noon_wide", PARTLY, 12, 45, 58, false },
    { "c4_afternoon", CLEAR, 15, 20, 4, false }, { "c5_dusk", PARTLY, 16, 58, 41, false }, { "c6_night", CLEAR, 23, 20, 15, false },
    { "c7_small_hours", CLEAR, 3, 5, 0, false }, { "c8_24h", CLEAR, 23, 59, 59, true }, { "c9_rain", RAIN, 10, 8, 20, false },
    { "c10_cloudy_24h", CLOUDY, 13, 7, 9, true }, { "c11_11am", CLEAR, 11, 11, 11, false },
  };
  for (auto& k : ck) {
    parseWeather(k.data, w); c.epoch = at(k.h, k.m, k.s); c.h24 = k.h24;
    for (int i = 0; i < 30; ++i) drawClockSky(c, w, true);
    dump(k.name);
  }
  { Weather none; c.epoch = at(18, 40, 3); c.h24 = false; for (int i = 0; i < 5; ++i) drawClockSky(c, none, true); dump("c12_no_weather_dusk"); }
  c.h24 = false;

  // ---- full-screen cover with the visualizer
  UiState ui; ui.art = art; ui.playing = true;
  VizState viz;
  const char* covers[] = { "art_orange", "art_blue", "art_magenta", "art_green", "art_gray", "art_paleblue", "demo_art" };
  const char* titles[] = { "Midnight Drive", "A Much Longer Song Title That Will Not Fit On One Line", "Paper Lanterns", "Fern", "Static", "Glass", "Demo" };
  int n = 0;
  for (const char* cv : covers) {
    if (!loadArt(cv)) { printf("missing %s\n", cv); continue; }
    ui.artValid = true; strcpy(ui.title, titles[n]); strcpy(ui.artist, "The Night Owls");
    artBackdropBlurred = true; musicPal = paletteFromArt(art); buildArtBlur(art);
    drawArtView(ui); vizReset(viz);
    for (int f = 0; f < 6; ++f) {
      for (int i = 0; i < VIZ_BARS; ++i) viz.target[i] = (uint8_t)(40 + (i * 53 + f * 31 + n * 17) % 200);
      drawVizBars(viz, true);
    }
    char name[40]; snprintf(name, sizeof name, "v%d_%s", ++n, cv); dump(name);
  }
  // bars falling to rest, then rising again (checks the erase and redraw paths)
  for (int f = 0; f < 40; ++f) drawVizBars(viz, false);
  dump("v8_bars_resting");
  for (int i = 0; i < VIZ_BARS; ++i) viz.target[i] = 255;
  drawVizBars(viz, true); dump("v9_bars_full");
  // no cover yet
  ui.artValid = false; memset(artBlur, 0x1C, sizeof artBlur); drawArtView(ui); vizReset(viz); drawVizBars(viz, false); dump("v10_no_cover");
  // Minimal look
  loadArt("art_blue"); ui.artValid = true; artBackdropBlurred = false; musicPal = SYSTEM_PALETTE; fullColorMode = false;
  drawArtView(ui); vizReset(viz);
  for (int i = 0; i < VIZ_BARS; ++i) viz.target[i] = (uint8_t)(30 + (i * 67) % 220);
  drawVizBars(viz, true); dump("v11_minimal");

  // ---- screensaver: floating cover
  fullColorMode = true; artBackdropBlurred = true; loadArt("art_magenta"); buildArtBlur(art); musicPal = paletteFromArt(art);
  FloatState fl; drawFloatStart(ui, fl); dump("f1_float_start");
  for (int i = 0; i < 137; ++i) drawFloatStep(ui, fl); dump("f2_float_137");
  for (int i = 0; i < 900; ++i) drawFloatStep(ui, fl); dump("f3_float_1037");
  artBackdropBlurred = false; FloatState fm; drawFloatStart(ui, fm); for (int i = 0; i < 300; ++i) drawFloatStep(ui, fm); dump("f4_float_minimal");

  // ---- screensaver: sky with drifting clock
  SaverState sv;
  parseWeather(CLEAR, w); c.epoch = at(23, 20, 15);
  drawSaverSky(c, w, sv, false); for (int i = 0; i < 200; ++i) drawSaverSky(c, w, sv, true); dump("s1_saver_night");
  parseWeather(RAIN, w); c.epoch = at(10, 8, 0);
  for (int i = 0; i < 300; ++i) drawSaverSky(c, w, sv, true); dump("s2_saver_rain");
  parseWeather(PARTLY, w); c.epoch = at(16, 55, 0);
  for (int i = 0; i < 300; ++i) drawSaverSky(c, w, sv, true); dump("s3_saver_dusk");
  fullColorMode = false;
  for (int i = 0; i < 300; ++i) drawSaverSky(c, w, sv, true); dump("s4_saver_minimal");
  sceneReady = false;                                         // as if the board had no memory for the sky
  SaverState sp; drawSaverSky(c, w, sp, false); for (int i = 0; i < 100; ++i) drawSaverSky(c, w, sp, true); dump("s5_saver_no_memory");
  sceneReady = true;

  // ---- settings pages
  Settings set; pal = SYSTEM_PALETTE; fullColorMode = false;
  drawSettingsScreen(set, 0); dump("x1_set_p1");
  drawSettingsScreen(set, 1); dump("x2_set_p2");
  drawSettingsScreen(set, 2); dump("x3_set_p3");
  set.cover = 1; set.visualizer = false; set.sunGlow = false; set.clockSky = false; set.saver = 0;
  fullColorMode = true; pal = appPalette(APP_SETTINGS);
  drawSettingsScreen(set, 0); dump("x4_set_p1_changed");
  drawSettingsScreen(set, 1); dump("x5_set_p2_changed");
  drawSettingsScreen(set, 2); dump("x6_set_p3_changed");
  // cover view with the bars turned off
  loadArt("art_blue"); ui.artValid = true; strcpy(ui.title, "Paper Lanterns"); artBackdropBlurred = true;
  musicPal = paletteFromArt(art); buildArtBlur(art); artvShift = 20; drawArtView(ui); dump("y1_cover_no_bars");
  artvShift = 0;
  // sunrise colors off: 4:50 PM is plain day
  sunGlowOn = false; parseWeather(PARTLY, w); c.epoch = at(16, 50); c.h24 = false;
  for (int i = 0; i < 10; ++i) drawWeatherSky(w, c, true); dump("y2_weather_glow_off");
  sunGlowOn = true;
  for (int i = 0; i < 10; ++i) drawWeatherSky(w, c, true); dump("y3_weather_glow_on");
  // moving sky off: the plain clock in its time-of-day colors
  clockSkyOn = false; pal = clockPalette(c.epoch); drawClockScreen(c); dump("y4_clock_sky_off");
  clockSkyOn = true;
  return 0;
}
