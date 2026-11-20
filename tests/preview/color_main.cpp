#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
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
  struct { const char* name; const char* data; int frames; } cases[] = {
    { "s1_clear", "OK\nCULVER CITY\n72\nF\nCLEAR\n0\n75\n56\n71\n6 MPH\n40%\n1:10 PM\nD\nTODAY|0|75|56\nTUE|1|70|54\nWED|4|64|52\n", 20 },
    { "s2_partly", "OK\nCULVER CITY\n68\nF\nPARTLY CLOUDY\n1\n72\n55\n66\n8 MPH\n61%\n7:42 AM\nD\nTODAY|1|72|55\nTUE|4|64|52\nWED|0|75|56\n", 60 },
    { "s3_rain", "OK\nCULVER CITY\n58\nF\nRAIN\n4\n61\n52\n55\n12 MPH\n88%\n3:05 PM\nD\nTODAY|4|61|52\nTUE|4|60|51\nWED|2|66|53\n", 25 },
    { "s4_storm", "OK\nCULVER CITY\n54\nF\nTHUNDERSTORM\n6\n57\n50\n50\n22 MPH\n93%\n9:15 PM\nN\nTODAY|6|57|50\nTUE|4|60|51\nWED|1|66|53\n", 12 },
    { "s5_storm_flash", nullptr, 1 },
    { "s6_snow", "OK\nMINNEAPOLIS\n-12\nC\nSNOW\n5\n-8\n-19\n-20\n34 KM/H\n90%\n23:05\nN\nTODAY|5|-8|-19\nTHU|5|-10|-21\nFRI|3|-2|-9\n", 40 },
    { "s7_fog", "OK\nSAN FRANCISCO\n55\nF\nFOG\n3\n60\n51\n54\n9 MPH\n97%\n6:30 AM\nD\nTODAY|3|60|51\nTUE|2|62|52\nWED|0|66|53\n", 30 },
    { "s8_cloudy", "OK\nSEATTLE\n104\nF\nCLOUDY\n2\n108\n84\n101\n5 MPH\n62%\n3:10 PM\nD\nTODAY|2|108|84\nSAT|2|99|80\nSUN|4|95|78\n", 30 },
    { "s9_night", "OK\nCULVER CITY\n61\nF\nCLEAR\n0\n72\n55\n60\n4 MPH\n70%\n11:20 PM\nN\nTODAY|0|72|55\nTUE|4|64|52\nWED|0|75|56\n", 30 },
  };
  for (auto& c : cases) {
    if (c.data) {
      parseWeather(c.data, w);
      float h, s; sceneHue(w.icon, w.night, h, s); pal = paletteFromHS(h, s);
      drawWeatherScreen(w);
    }
    for (int i = 0; i < c.frames; ++i) drawWeatherSky(w, true);
    if (!c.data) { while (weatherScene.flash == 0) drawWeatherSky(w, true); }
    char p[64]; snprintf(p, sizeof p, "%s.ppm", c.name); dump(p);
  }
  fullColorMode = false; pal = SYSTEM_PALETTE; drawWeatherScreen(w); dump("t0_minimal_weather.ppm");
  fullColorMode = true;
  pal = menuPalette(); drawMenu(APP_WEATHER); dump("t1_menu.ppm");
  ClockState c; c.known = true;
  uint32_t base = 1794787200u;   // a midnight
  int hours[] = { 6, 13, 18, 23 };
  for (int i = 0; i < 4; ++i) { c.epoch = base + hours[i] * 3600 + 24 * 60 + 37; pal = clockPalette(c.epoch); drawClockScreen(c); char p[32]; snprintf(p, sizeof p, "t%d_clock.ppm", i + 2); dump(p); }
  ReminderList r; parseReminders("3\n0|9:00 AM|Water the plants\n0||Order a display stand\n0|5:30 PM|Pick up prints\n1||Charge headphones\n", r);
  pal = appPalette(APP_REMINDERS); drawRemindersScreen(r, 0); dump("u1_rem.ppm");
  EventList e; parseEvents("OK\nTODAY|9:30 AM|Demo: meeting\nTODAY|ALL DAY|Demo: all-day item\nTOMORROW|2:00 PM|Demo: appointment\n", e);
  pal = appPalette(APP_CALENDAR); drawCalendarScreen(e, 0); dump("u2_cal.ppm");
  Settings set; Stats st; parseStats("37\n62\n91\n58\n-\n", st); st.chipTemp = 47; st.uptimeSec = 8045; st.freeKb = 212; st.linkUp = true;
  pal = appPalette(APP_SYSTEM); drawSystemScreen(st, set); dump("u3_sys.ppm");
  pal = appPalette(APP_SETTINGS); drawSettingsScreen(set, 0); dump("u4_set.ppm"); drawSettingsScreen(set, 1); dump("u5_set2.ppm");
  set.fullColor = false; fullColorMode = false; pal = SYSTEM_PALETTE; drawSettingsScreen(set, 0); dump("u6_set_minimal.ppm");
  drawMenu(APP_WEATHER); dump("u7_menu_minimal.ppm");
  return 0;
}
