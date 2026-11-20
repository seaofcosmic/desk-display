#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
static LGFX_Sprite canvas;
static void dump(const char* name) {
  char p[80]; snprintf(p, sizeof p, "%s.ppm", name);
  FILE* f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; ++y) for (int x = 0; x < SCREEN_W; ++x) {
    uint16_t c = canvas.readPixel(x, y);
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static uint32_t at(int h, int m) { return 1794787200u / 86400 * 86400 + h * 3600 + m * 60; }
int main() {
  canvas.setColorDepth(16); canvas.createSprite(SCREEN_W, SCREEN_H); uiBegin(&canvas); sceneBegin(&canvas);
  fullColorMode = true;
  Weather w; ClockState c; c.known = true; c.epoch = at(10, 8);
  const char* RAIN  = "OK\nCULVER CITY\n14\nC\nRAIN\n4\n16\n11\n13\n19 KM/H\n88%\n3:05 PM\nD|387|1012\nTODAY|4|16|11\nTUE|4|16|11\nWED|2|19|12\n";
  const char* STORM = "OK\nCULVER CITY\n13\nC\nTHUNDERSTORM\n6\n15\n10\n12\n24 KM/H\n93%\n3:05 PM\nD|387|1012\nTODAY|6|15|10\nTUE|4|16|11\nWED|2|19|12\n";
  const char* SNOW  = "OK\nCULVER CITY\n-2\nC\nSNOW\n5\n0\n-4\n-6\n10 KM/H\n80%\n3:05 PM\nD|387|1012\nTODAY|5|0|-4\nTUE|4|16|11\nWED|2|19|12\n";
  parseWeather(RAIN, w);
  drawWeatherScreen(w, c); for (int i = 0; i < 40; ++i) drawWeatherSky(w, c, true); dump("r1_rain");
  for (int i = 0; i < 300; ++i) drawWeatherSky(w, c, true); dump("r2_rain_later");
  for (int i = 0; i < 20; ++i) drawClockSky(c, w, true); dump("r3_clock_rain");
  parseWeather(STORM, w); for (int i = 0; i < 60; ++i) drawWeatherSky(w, c, true); dump("r4_storm");
  parseWeather(SNOW, w); for (int i = 0; i < 40; ++i) drawWeatherSky(w, c, true); dump("r5_snow_unchanged");
  return 0;
}
