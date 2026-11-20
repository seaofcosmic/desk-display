// Renders the firmware's real drawing code (ui.h) into image files on a computer.
#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/ui.h"
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
int main(int argc, char** argv) {
  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  uiBegin(&canvas);
  static std::vector<uint8_t> art(ART_BYTES);
  if (argc > 1) { FILE* f = fopen(argv[1], "rb"); if (f) { if (fread(art.data(), 1, ART_BYTES, f) != ART_BYTES) return 2; fclose(f); } }

  UiState s; s.art = art.data(); s.artValid = true;
  strcpy(s.title, "Fury Road"); strcpy(s.artist, "The Wasteland Radio"); strcpy(s.album, "Guzzoline Sessions");
  s.playing = true; s.hasPos = true; s.canSeek = true; s.posMs = 84000; s.durMs = 201000;
  drawPlayingScreen(s); dump("1_playing.ppm");

  strcpy(s.title, "A Very Long Song Title That Needs To Wrap Across Several Lines Without Breaking");
  strcpy(s.artist, "Artist With A Long Name Too And Then Some More"); strcpy(s.album, "Deluxe Anniversary Edition (Remastered 2011)");
  s.playing = false; s.artValid = false; s.posMs = 3725000; s.durMs = 3725000; s.pressed = ZONE_NEXT;
  drawPlayingScreen(s); dump("2_long_paused.ppm");

  strcpy(s.title, "Supercalifragilisticexpialidociousness"); strcpy(s.artist, ""); strcpy(s.album, "");
  s.playing = true; s.hasPos = false; s.durMs = 0; s.posMs = 0; s.pressed = ZONE_PLAY; s.artValid = true;
  drawPlayingScreen(s); dump("3_nopos.ppm");

  drawNoLink(); dump("4_nolink.ppm");
  drawStandby(); dump("5_standby.ppm");
  drawMessageScreen("TOUCH SETUP", "Lift your finger, then press", "the tip of each corner arrow."); dump("6_touch.ppm");
  drawMessageScreen("DESK DISPLAY", "Starting up.", "Hold the screen to redo touch setup."); dump("7_boot.ppm");
  return 0;
}
