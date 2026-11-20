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
  fprintf(stderr, "[%6u] screenshot %s  (app=%d menu=%d artView=%d saver=%d float=%d playing=%d)\n", millis(), name, (int)app, menuOpen, artView, saverOn, saverFloat, ui.playing);
}
int main(int argc, char** argv) {
  Serial.fd = atoi(argv[1]);
  touchScript = {
    { 2500, 2600, 200, 120, 200, 120 },    // tap: dismiss an alert if one is up
    { 4000, 4300, 230, 40, 235, 150 },     // swipe down: menu
    { 5500, 5600, 57, 197, 57, 197 },      // SETTINGS
    { 7000, 7100, 200, 150, 200, 150 },    // Visualizer bars: off
    { 8500, 8800, 230, 40, 235, 150 },     // swipe down: menu
    { 10000, 10100, 57, 61, 57, 61 },      // MUSIC
    { 17000, 17300, 230, 40, 235, 150 },   // swipe down: menu
    { 18500, 18600, 160, 61, 160, 61 },    // CLOCK
  };
  struct { uint32_t at; const char* name; bool done; } shots[] = {
    { 6500, "s1_settings.ppm", false }, { 7600, "s2_viz_off.ppm", false }, { 15500, "s3_cover_no_bars.ppm", false },
    { 20000, "s4_clock.ppm", false },
  };
  setup();
  uint32_t vizFrames = 0, lastViz = 0, vizLate = 0;
  while (millis() < 21000) {
    loop();
    if (viz.lastDataMs != lastViz) { lastViz = viz.lastDataMs; ++vizFrames; if (millis() > 7100) ++vizLate; }
    for (auto& s : shots) if (!s.done && millis() >= s.at) { shot(s.name); s.done = true; }
    usleep(500);
  }
  fprintf(stderr, "visualizer frames received: %u, after turning the bars off: %u\n", vizFrames, vizLate);
  return 0;
}
