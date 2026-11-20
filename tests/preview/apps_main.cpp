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
  canvas.setColorDepth(16); canvas.createSprite(SCREEN_W, SCREEN_H); uiBegin(&canvas);
  drawMenu(APP_MUSIC); drawMenuTile(2, false, true); dump("m1_menu.ppm");
  drawMenu(APP_REMINDERS); dump("m2_menu.ppm");
  ClockState c; c.known = true; c.epoch = 1794816000u + 7*3600 + 59*60 + 42; drawClockScreen(c); dump("m3_clock.ppm");
  c.epoch = 1794816000u + 12*3600 + 48*60 + 7; drawClockScreen(c); dump("m4_clock1248.ppm");
  c.h24 = true; c.epoch += 9*3600; drawClockScreen(c); dump("m5_clock24.ppm");
  Weather w;
  parseWeather("OK\nCULVER CITY\n68\nF\nPARTLY CLOUDY\n1\n72\n55\n66\n8 MPH\n61%\n7:42 AM\nTODAY|1|72|55\nTUE|4|64|52\nWED|0|75|56\n", w); drawWeatherScreen(w); dump("m6_weather.ppm");
  parseWeather("OK\nMINNEAPOLIS\n-12\nC\nTHUNDERSTORM\n6\n-8\n-19\n-20\n34 KM/H\n100%\n23:05\nTODAY|6|-8|-19\nTHU|5|-10|-21\nFRI|3|-2|-9\n", w); drawWeatherScreen(w); dump("m7_weather2.ppm");
  parseWeather("OK\nPHOENIX\n104\nF\nCLEAR\n0\n108\n84\n101\n5 MPH\n9%\n3:10 PM\nTODAY|0|108|84\nSAT|2|99|80\nSUN|4|95|78\n", w); drawWeatherScreen(w); dump("m8_weather3.ppm");
  parseWeather("ERR\nSet your city in settings.json\n", w); drawWeatherScreen(w); dump("m9_weather_err.ppm");
  ReminderList r;
  parseReminders("4\n0|9:00 AM|Send USAR deck revisions\n0||Email the invoice to the client before the end of the day today\n0|3:30 PM|Pick up prints\n0||Water the plants\n1||Order display stand\n1|8:15 AM|Call the dentist\n1||Back up laptop\n", r);
  drawRemindersScreen(r, 0); dump("n1_rem.ppm"); drawRemindersScreen(r, 1); dump("n2_rem_p2.ppm");
  parseReminders("0\n", r); drawRemindersScreen(r, 0); dump("n3_rem_empty.ppm");
  Alert a; parseAlert("3:30 PM\nPick up prints from the shop on Washington before they close\n", a); drawAlert(a); dump("n4_alert.ppm");
  drawNoLink(); dump("n5_nolink.ppm");
  return 0;
}
