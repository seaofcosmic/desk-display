#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
#include <chrono>
static LGFX_Sprite canvas;
static void dump(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; ++y) for (int x = 0; x < SCREEN_W; ++x) {
    uint16_t c = canvas.readPixel(x, y);
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
int main() {
  canvas.setColorDepth(16); canvas.createSprite(SCREEN_W, SCREEN_H); uiBegin(&canvas); sceneBegin(&canvas);
  fullColorMode = true;
  Weather w;
  struct { const char* name; const char* data; int frames; bool flash; } cases[] = {
    { "a1_clear", "OK\nCULVER CITY\n72\nF\nCLEAR\n0\n75\n56\n71\n6 MPH\n40%\n1:10 PM\nD\nTODAY|0|75|56\nTUE|1|70|54\nWED|4|64|52\n", 20, false },
    { "a2_partly", "OK\nCULVER CITY\n68\nF\nPARTLY CLOUDY\n1\n72\n55\n66\n8 MPH\n61%\n7:42 AM\nD\nTODAY|1|72|55\nTUE|4|64|52\nWED|0|75|56\n", 60, false },
    { "a3_rain", "OK\nCULVER CITY\n58\nF\nRAIN\n4\n61\n52\n55\n12 MPH\n88%\n3:05 PM\nD\nTODAY|4|61|52\nTUE|4|60|51\nWED|2|66|53\n", 25, false },
    { "a4_storm", "OK\nCULVER CITY\n54\nF\nTHUNDERSTORM\n6\n57\n50\n50\n22 MPH\n93%\n9:15 PM\nN\nTODAY|6|57|50\nTUE|4|60|51\nWED|1|66|53\n", 12, false },
    { "a5_storm_flash", nullptr, 1, true },
    { "a6_snow", "OK\nMINNEAPOLIS\n-12\nC\nSNOW\n5\n-8\n-19\n-20\n34 KM/H\n90%\n23:05\nN\nTODAY|5|-8|-19\nTHU|5|-10|-21\nFRI|3|-2|-9\n", 40, false },
    { "a7_fog", "OK\nSAN FRANCISCO\n55\nF\nFOG\n3\n60\n51\n54\n9 MPH\n97%\n6:30 AM\nD\nTODAY|3|60|51\nTUE|2|62|52\nWED|0|66|53\n", 30, false },
    { "a8_cloudy", "OK\nSEATTLE\n104\nF\nCLOUDY\n2\n108\n84\n101\n5 MPH\n62%\n3:10 PM\nD\nTODAY|2|108|84\nSAT|2|99|80\nSUN|4|95|78\n", 30, false },
    { "a9_night", "OK\nCULVER CITY\n61\nF\nCLEAR\n0\n72\n55\n60\n4 MPH\n70%\n11:20 PM\nN\nTODAY|0|72|55\nTUE|4|64|52\nWED|0|75|56\n", 30, false },
  };
  int n = 0;
  for (auto& c : cases) {
    if (c.data) { parseWeather(c.data, w); drawWeatherScreen(w); }
    for (int i = 0; i < c.frames; ++i) drawWeatherSky(w, true);
    if (c.flash) { while (weatherScene.flash == 0) drawWeatherSky(w, true); }
    char p[64]; snprintf(p, sizeof p, "%s.ppm", c.name); dump(p);
    if (c.data && (n == 0 || n == 1 || n == 2 || n == 3 || n == 8)) {
      auto t0 = std::chrono::steady_clock::now();
      drawWeatherDetails(w);
      auto ms = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
      snprintf(p, sizeof p, "b%d_details.ppm", n + 1); dump(p);
      printf("%s details drawn in %.1f ms on this PC\n", c.name, ms / 1000.0);
    }
    ++n;
  }
  return 0;
}
