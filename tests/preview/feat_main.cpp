#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
#include <vector>
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
  static std::vector<uint8_t> art(ART_BYTES);
  const char* names[] = { "orange", "blue", "magenta", "green", "gray", "paleblue" };
  UiState s; s.art = art.data(); s.artValid = true;
  strcpy(s.title, "Fury Road"); strcpy(s.artist, "The Wasteland Radio"); strcpy(s.album, "Guzzoline Sessions");
  s.playing = true; s.hasPos = true; s.canSeek = true; s.posMs = 84000; s.durMs = 201000;
  for (int i = 0; i < 6; ++i) {
    char p[64]; snprintf(p, sizeof p, "art_%s.rgb565", names[i]);
    FILE* f = fopen(p, "rb"); if (fread(art.data(), 1, ART_BYTES, f) != ART_BYTES) return 2; fclose(f);
    musicPal = paletteFromArt(art.data());
    printf("%s bg=%06x rust=%06x dim=%06x accent=%06x cream=%06x\n", names[i], musicPal.bg, musicPal.rust, musicPal.dim, musicPal.accent, musicPal.cream);
    s.pressed = (i == 2) ? ZONE_NEXT : ZONE_NONE; s.playing = (i != 4);
    drawPlayingScreen(s); snprintf(p, sizeof p, "p%d_%s.ppm", i, names[i]); dump(p);
  }
  musicPal = SYSTEM_PALETTE;
  drawMenu(APP_MUSIC); drawMenuTile(4, false, true); dump("q1_menu.ppm");
  drawMenu(APP_SETTINGS); dump("q2_menu.ppm");
  EventList e;
  parseEvents("OK\nTODAY|9:30 AM|Design review with the print vendor about the insert proofs\nTODAY|ALL DAY|Submit portfolio update\nTOMORROW|2:00 PM|Dentist\nWED 18|ALL DAY|Trash day\nWED 18|11:15 AM|Coffee with Sam\nFRI 20|6:00 PM|Dinner\n", e);
  drawCalendarScreen(e, 0); dump("q3_cal.ppm"); drawCalendarScreen(e, 1); dump("q4_cal2.ppm");
  parseEvents("OK\n", e); drawCalendarScreen(e, 0); dump("q5_cal_empty.ppm");
  parseEvents("ERR\nAdd your Notion token to settings.json\n", e); drawCalendarScreen(e, 0); dump("q6_cal_err.ppm");
  Settings set; Stats st;
  drawSystemScreen(st, set); dump("r1_sys_empty.ppm");
  parseStats("37\n62\n81\n58\n-\n", st); st.chipTemp = 47; st.uptimeSec = 8045; st.freeKb = 212; st.linkUp = true;
  drawSystemScreen(st, set); dump("r2_sys.ppm");
  parseStats("100\n93\n8\n91\n77\n", st); set.statsF = true; st.chipTemp = STAT_UNKNOWN; drawSystemValues(st, set); dump("r3_sys_hot.ppm");
  drawSettingsScreen(set, 0); dump("r4_set.ppm");
  set.albumColors = false; set.h24 = true; set.brightness = 1; drawSettingsScreen(set, 1); dump("r5_set2.ppm");
  return 0;
}
