#include <LovyanGFX.hpp>
#include "../../desk-display/firmware/DeskDisplay/apps.h"
#include <cstdio>
static LGFX_Sprite canvas;
static void dump(const char* name) { char p[80]; snprintf(p, sizeof p, "%s.ppm", name); FILE* f = fopen(p, "wb"); fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) { uint16_t c = canvas.readPixel(x, y); uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) }; fwrite(rgb, 1, 3, f); } fclose(f); }
int main() { canvas.setColorDepth(16); canvas.createSprite(320, 240); uiBegin(&canvas);
  pal = SYSTEM_PALETTE; drawStandby(); dump("o1_minimal"); drawOpenButton(true); dump("o2_minimal_tapped");
  fullColorMode = true; pal = appPalette(APP_MUSIC); drawStandby(); dump("o3_color");
  printf("button hit test: %d %d\n", openButtonAt(160, 167), openButtonAt(160, 100)); return 0; }
