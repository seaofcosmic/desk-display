#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
static LGFX_Sprite canvas;
static void dump(const char* name) { char p[80]; snprintf(p, sizeof p, "%s.ppm", name); FILE* f = fopen(p, "wb"); fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) { uint16_t c = canvas.readPixel(x, y); uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) }; fwrite(rgb, 1, 3, f); } fclose(f); }
static uint8_t art[ART_BYTES];
static Lyrics L;
int main() {
  canvas.setColorDepth(16); canvas.createSprite(320, 240); uiBegin(&canvas);
  FILE* f = fopen("../preview/art_blue.rgb565", "rb"); fread(art, 1, ART_BYTES, f); fclose(f);
  UiState ui; ui.art = art; ui.artValid = true; ui.playing = true; ui.hasPos = true; ui.canSeek = false; ui.durMs = 200000; ui.posMs = 30000;
  strcpy(ui.title, "Paper Lanterns Over The Quiet Harbor Tonight"); strcpy(ui.artist, "The Night Owls"); strcpy(ui.album, "First Light");
  // made-up lyrics
  const char* P = "S\n0|\n12000|We walked the long way home tonight\n15500|The streetlights humming soft and low\n19000|Your jacket on my shoulders, warm\n22500|And nowhere that we had to go\n26000|\n30000|Paper lanterns over the harbor, drifting out past where the water meets the sky\n36000|Count them one by one\n40000|Till the morning comes";
  parseLyrics(P, strlen(P), L);
  printf("lines=%d state=%d line@31s=%d\n", L.count, L.state, lyricsLineAt(L, 31000, 200000));
  // controls with the new button, both looks
  musicPal = paletteFromArt(art); drawPlayingScreen(ui); dump("l1_controls_color");
  musicPal = SYSTEM_PALETTE; drawPlayingScreen(ui); dump("l2_controls_minimal");
  // lyrics view, Full color
  musicPal = paletteFromArt(art); buildArtBlur(art); artBackdropBlurred = true;
  artBackdrop(0,0,320,240); drawLyricsHeader(ui);
  drawLyricsLines(L, lyricsLineAt(L, 20000, 200000)); dump("l3_lyrics_color");
  drawLyricsLines(L, lyricsLineAt(L, 31000, 200000)); dump("l4_lyrics_long_line");
  drawLyricsLines(L, lyricsLineAt(L, 5000, 200000)); dump("l5_before_singing");
  Lyrics W; parseLyrics("W", 1, W); drawLyricsLines(W, -1); dump("l6_waiting");
  // Minimal
  artBackdropBlurred = false; musicPal = SYSTEM_PALETTE;
  artBackdrop(0,0,320,240); drawLyricsHeader(ui); drawLyricsLines(L, lyricsLineAt(L, 16000, 200000)); dump("l7_lyrics_minimal");
  Lyrics N; parseLyrics("N", 1, N); drawLyricsLines(N, -1); dump("l8_none_minimal");
  const char* U = "P\n0|Line one without timing\n0|Line two\n0|Line three\n0|Line four";
  Lyrics Un; parseLyrics(U, strlen(U), Un); drawLyricsLines(Un, lyricsLineAt(Un, 100000, 200000)); dump("l9_untimed");
  printf("hit: %d %d\n", lyricsButtonAt(200, 130), lyricsButtonAt(200, 90));
}
