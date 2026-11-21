#include "DeskDisplay.ino"
#include <string>
static void shot(const char* name) { FILE* f = fopen(name, "wb"); fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) { uint16_t c = lcd.readPixel(x, y); uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) }; fwrite(rgb, 1, 3, f); } fclose(f);
  fprintf(stderr, "[%6u] %s lyricsView=%d artView=%d shown=%d state=%d\n", millis(), name, lyricsView, artView, lyricsShown, lyrics.state); }
static void feed(char type, const std::string& p) { std::string f = "DT"; f += type; uint32_t n = p.size(); uint16_t sum = 0;
  for (int i = 0; i < 4; ++i) f += (char)((n >> (8 * i)) & 255); for (unsigned char c : p) sum += c; f += p; f += (char)(sum & 255); f += (char)(sum >> 8);
  for (unsigned char c : f) if (parser->feed(c)) handleFrame(); }
static std::string state(uint32_t pos) { std::string p; p += (char)(1 | 2); for (int i = 0; i < 4; ++i) p += (char)((pos >> (8 * i)) & 255); uint32_t d = 200000; for (int i = 0; i < 4; ++i) p += (char)((d >> (8 * i)) & 255); return p; }
int main() {
  touchScript = { { 3000, 3100, 200, 130, 200, 130 },     // tap LYRICS
                  { 9000, 9100, 160, 120, 160, 120 } };   // tap: back to the controls
  setup();
  const char* L1 = "S\n0|\n2000|First made up line\n4500|Second made up line\n7000|Third made up line";
  uint32_t lastS = 0; bool sentL = false; int shots = 0;
  while (millis() < 11000) {
    uint32_t now = millis();
    if (now > 500 && !lastS) { feed('M', "Some Song\nSome Artist\nSome Album"); }
    if (now > 500 && now - lastS >= 500) { lastS = now; feed('S', state(now - 500)); }
    if (now > 4000 && !sentL) { sentL = true; feed('L', L1); }
    loop();
    static const uint32_t at[] = { 2500, 3600, 4300, 6000, 8200, 9800 };
    static const char* nm[] = { "y1_controls.ppm", "y2_waiting.ppm", "y3_lyrics_arrived.ppm", "y4_line2.ppm", "y5_line3.ppm", "y6_back.ppm" };
    if (shots < 6 && now >= at[shots]) { shot(nm[shots]); ++shots; }
    usleep(500);
  }
}
