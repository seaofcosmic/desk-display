#include <LovyanGFX.hpp>
#include <cstdio>
int main() {
  LGFX_Sprite canvas; canvas.setColorDepth(16); canvas.createSprite(64, 32);
  LGFX_Sprite sp(&canvas); sp.setColorDepth(4); sp.createSprite(64, 32); sp.createPalette();
  sp.setPaletteColor(0, 0x1E6FD9u); sp.setPaletteColor(3, 0xFFD23Fu); sp.setPaletteColor(9, 0xFFFFFFu);
  sp.fillScreen(0); sp.fillRect(4, 4, 10, 10, 3); sp.setFont(&fonts::Font2); sp.setTextColor(9); sp.drawString("Hi", 30, 4);
  sp.pushSprite(0, 0);
  printf("bg %04x rect %04x\n", canvas.readPixel(0, 0), canvas.readPixel(6, 6));
  int white = 0; for (int y = 0; y < 32; ++y) for (int x = 28; x < 64; ++x) if (canvas.readPixel(x, y) == 0xFFFF) ++white;
  printf("white text pixels %d, sprite bytes %d\n", white, (int)sp.bufferLength());
  sp.setPaletteColor(0, 0xFF0000u); sp.pushSprite(0, 0); printf("bg after palette change %04x\n", canvas.readPixel(0, 0));
  return 0;
}
