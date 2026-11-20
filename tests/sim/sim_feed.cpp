// The real firmware with messages fed straight into it (no companion), to reach cases the demo does not.
#include "DeskDisplay.ino"
#include <string>
static void shot(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
    uint16_t c = lcd.readPixel(x, y);
    uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  fprintf(stderr, "[%6u] screenshot %s  (app=%d artView=%d saver=%d float=%d alert=%d link=%d track=%d playing=%d artValid=%d)\n", millis(), name, (int)app, artView, saverOn, saverFloat, alertOpen, linkUp, hasTrack, ui.playing, ui.artValid);
}
static void feed(char type, const std::string& payload) {
  std::string f = "DT"; f += type;
  uint32_t n = payload.size(); uint16_t sum = 0;
  for (int i = 0; i < 4; ++i) f += (char)((n >> (8 * i)) & 255);
  for (unsigned char c : payload) sum += c;
  f += payload; f += (char)(sum & 255); f += (char)(sum >> 8);
  for (unsigned char c : f) if (parser->feed(c)) handleFrame();
}
static std::string state(bool playing, uint32_t pos, uint32_t dur) {
  std::string p; p += (char)((playing ? 1 : 0) | 2 | 4);
  for (int i = 0; i < 4; ++i) p += (char)((pos >> (8 * i)) & 255);
  for (int i = 0; i < 4; ++i) p += (char)((dur >> (8 * i)) & 255);
  return p;
}
static std::string art(const char* name) {
  char p[160]; snprintf(p, sizeof p, "/home/claude/tests/preview/%s.rgb565", name);
  std::string d(ART_BYTES, 0); FILE* f = fopen(p, "rb"); if (fread(&d[0], 1, ART_BYTES, f) != ART_BYTES) fprintf(stderr, "bad art\n"); fclose(f); return d;
}
int main(int argc, char** argv) {
  bool minimal = argc > 1;
  const char* pre = minimal ? "m" : "f";
  touchScript = { { 18500, 18600, 160, 120, 160, 120 } };      // tap: dismiss the alert
  bool playing = true; bool feeding = true;
  struct Ev { uint32_t at; int what; bool done; } evs[] = {
    { 500, 0, false }, { 5000, 1, false }, { 5800, 2, false }, { 12000, 3, false }, { 12800, 4, false },
    { 15000, 5, false }, { 17000, 6, false }, { 20000, 7, false }, { 22000, 8, false },
  };
  struct { uint32_t at; const char* name; bool done; } shots[] = {
    { 2000, "a_controls", false }, { 4500, "b_cover", false }, { 5300, "c_cover_waiting_for_art", false }, { 6400, "d_cover_new_track", false },
    { 11000, "e_saver_float", false }, { 12400, "f_float_waiting_for_art", false }, { 13800, "g_float_new_track", false },
    { 16200, "h_saver_sky_after_pause", false }, { 17600, "i_alert_over_saver", false }, { 19200, "j_after_alert", false },
    { 20600, "k_nothing_playing", false }, { 30500, "l_link_lost", false },
  };
  setup();
  if (minimal) { settings.fullColor = false; fullColorMode = false; refreshMusicPalette(); showCurrent(); }
  uint32_t lastS = 0, lastV = 0; uint32_t pos = 0;
  while (millis() < 31000) {
    uint32_t now = millis();
    for (auto& e : evs) if (!e.done && now >= e.at) {
      e.done = true;
      switch (e.what) {
        case 0: { std::string t; uint32_t ep = 1794787200u / 86400 * 86400 + 23 * 3600 + 20 * 60; for (int i = 0; i < 4; ++i) t += (char)((ep >> (8 * i)) & 255); t += (char)0;
                  feed('T', t); feed('M', "Midnight Drive\nThe Night Owls\nFirst Light"); feed('S', state(true, 0, 200000)); feed('A', art("art_orange"));
                  feed('W', "OK\nCULVER CITY\n61\nF\nCLEAR\n0\n72\n55\n60\n4 MPH\n70%\n11:20 PM\nN|387|1012\nTODAY|0|72|55\nTUE|4|64|52\nWED|0|75|56\n"); break; }
        case 1: feed('M', "Paper Lanterns\nThe Night Owls\nFirst Light"); pos = 0; break;
        case 2: feed('A', art("art_blue")); break;
        case 3: feed('M', "Glass\nThe Night Owls\nFirst Light"); pos = 0; break;
        case 4: feed('A', art("art_magenta")); break;
        case 5: playing = false; feed('S', state(false, pos, 200000)); break;
        case 6: feed('N', "11:30 PM\nCall the printer about the proofs"); break;
        case 7: feed('X', ""); playing = false; break;
        case 8: feeding = false; break;
      }
    }
    if (feeding && now - lastS >= 1000 && now > 600 && now < 20000) { lastS = now; pos += 1000; feed('S', state(playing, pos, 200000)); }
    if (feeding && now >= 20000 && now - lastS >= 1000) { lastS = now; feed('X', ""); }
    if (feeding && playing && now - lastV >= 70 && now > 600) { lastV = now; std::string v; for (int i = 0; i < 16; ++i) v += (char)(40 + ((now / 70) * 37 + i * 53) % 200); feed('V', v); }
    loop();
    for (auto& s : shots) if (!s.done && millis() >= s.at) { char n[80]; snprintf(n, sizeof n, "%s_%s.ppm", pre, s.name); shot(n); s.done = true; }
    usleep(500);
  }
  return 0;
}
