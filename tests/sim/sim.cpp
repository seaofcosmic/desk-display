#include "DeskDisplay.ino"
static void shot(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
    uint16_t c = lcd.readPixel(x, y);
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  fprintf(stderr, "[%6u] screenshot %s\n", millis(), name);
}
int main(int argc, char** argv) {
  Serial.fd = atoi(argv[1]);
  touchScript = {
    { 2000, 2100, 160, 120, 160, 120 },    // tap: dismiss the event alert
    { 4000, 4300, 230, 40, 235, 150 },     // swipe down: menu
    { 5500, 5600, 263, 61, 263, 61 },      // tap WEATHER: the full-screen sky
    { 13500, 13800, 160, 190, 160, 70 },   // swipe up: details over the blurred sky
    { 19000, 19300, 160, 60, 160, 180 },   // swipe down: back to the sky
    { 29000, 29300, 160, 60, 160, 180 },   // swipe down again: menu
    { 31000, 31100, 57, 197, 57, 197 },    // tap SETTINGS
    { 32500, 32600, 200, 46, 200, 46 },    // Look -> MINIMAL
    { 34000, 34300, 160, 40, 160, 150 },   // swipe down: menu
    { 35000, 35100, 263, 61, 263, 61 },    // tap WEATHER (minimal)
    { 37500, 37800, 160, 190, 160, 70 },   // swipe up in minimal: nothing should change
  };
  struct { uint32_t at; const char* name; bool done; } shots[] = {
    { 7000, "a_sky_clear.ppm", false }, { 12500, "b_sky_partly.ppm", false }, { 14800, "c_details_partly.ppm", false },
    { 18300, "d_details_after_weather_changed.ppm", false }, { 20800, "e_sky_rain.ppm", false }, { 27700, "f_sky_storm.ppm", false },
    { 30200, "g_menu.ppm", false }, { 33500, "h_settings_minimal.ppm", false }, { 36800, "i_weather_minimal.ppm", false },
    { 38800, "j_weather_minimal_after_swipe_up.ppm", false },
  };
  setup();
  while (millis() < 39800) {
    loop();
    for (auto& s : shots) if (!s.done && millis() >= s.at) { shot(s.name); s.done = true; }
    usleep(500);
  }
  return 0;
}
