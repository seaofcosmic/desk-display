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
    { 2500, 2600, 200, 120, 200, 120 },    // tap: dismiss the alert if one is up
    { 9000, 9100, 160, 100, 160, 100 },    // tap the full-screen cover: controls come back
    { 10500, 10600, 60, 60, 60, 60 },      // tap the cover: full screen again
    { 31000, 31100, 160, 120, 160, 120 },  // touch during the screensaver: only wakes it
    { 33000, 33100, 160, 120, 160, 120 },  // tap: controls
    { 34000, 34150, 264, 216, 264, 216 },  // NEXT
    { 35800, 35950, 160, 216, 160, 216 },  // PAUSE
    { 36500, 36800, 230, 40, 235, 150 },   // swipe down: menu
    { 38000, 38100, 160, 61, 160, 61 },    // tap CLOCK
    { 61000, 61100, 160, 120, 160, 120 },  // touch during the sky screensaver
    { 63000, 63300, 230, 40, 235, 150 },   // swipe down: menu
    { 64500, 64600, 57, 197, 57, 197 },    // tap SETTINGS
    { 66000, 66100, 240, 225, 240, 225 },  // footer: page 2
    { 67500, 67600, 200, 190, 200, 190 },  // Screensaver row: 5 MIN -> 15 MIN
  };
  struct { uint32_t at; const char* name; bool done; } shots[] = {
    { 5500, "a_controls.ppm", false }, { 8000, "b_cover_after_idle.ppm", false }, { 9700, "c_controls_after_tap.ppm", false },
    { 11500, "d_cover_by_tap.ppm", false }, { 14000, "e_cover_later.ppm", false }, { 27500, "f_saver_float.ppm", false },
    { 30500, "g_saver_float_later.ppm", false }, { 31900, "h_woke_to_cover.ppm", false }, { 33600, "i_controls.ppm", false },
    { 35400, "j_next_track.ppm", false }, { 36300, "k_paused.ppm", false }, { 39500, "l_clock_sky.ppm", false },
    { 47000, "m_clock_sky_later.ppm", false }, { 55500, "n_saver_sky.ppm", false }, { 60500, "o_saver_sky_later.ppm", false },
    { 62000, "p_woke_to_clock.ppm", false }, { 65500, "q_settings.ppm", false }, { 68600, "r_settings_p2.ppm", false },
  };
  setup();
  uint32_t vizFrames = 0, lastViz = 0;
  while (millis() < 69500) {
    loop();
    if (viz.lastDataMs != lastViz) { lastViz = viz.lastDataMs; ++vizFrames; }
    for (auto& s : shots) if (!s.done && millis() >= s.at) { shot(s.name); s.done = true; }
    usleep(500);
  }
  fprintf(stderr, "visualizer frames received: %u\n", vizFrames);
  return 0;
}
