#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include "../sprite_scene.hpp"
#include "../page_commands.hpp"

// Software rasterizer for X2Tile rectangles (exactly emulating RDP texture rectangle)
static void rasterize_tile(uint16_t* out, const uint8_t* vram, const uint16_t* pal, const X2Tile& t) {
 X2RectWords r = x2_rect_words(t);
 int x0 = ((r.w[1] >> 12) & 4095) / 4, y0 = (r.w[1] & 4095) / 4;
 int width = ((r.w[0] >> 12) & 4095) / 4 - x0, height = (r.w[0] & 4095) / 4 - y0;
 int sx = (int16_t)(r.w[2] >> 16) / 32, sy = (int16_t)r.w[2] / 32;
 int dx = (int16_t)(r.w[3] >> 16) / 1024, dy = (int16_t)r.w[3] / 1024;
 unsigned p = (r.w[1] >> 24) & 7;
 unsigned page = (t.attr & 2047) >> 5;

 for (int y = 0; y < height; y++) {
  for (int x = 0; x < width; x++) {
   int u = sx + x * dx, vv = sy + y * dy;
   assert(u >= 0 && u < 8 && vv >= 0 && vv < 256);
   unsigned nib = (vram[page * 1024 + vv * 4 + u / 2] >> ((u & 1) ? 0 : 4)) & 15;
   if (nib) {
    out[(y0 + y) * 320 + x0 + x] = pal[p * 16 + nib];
   }
  }
 }
}

int main() {
 printf("Running Native Sprite Fidelity Tests...\n");
 unsigned rng = 0xDEADBEEF;
 uint16_t pal[256];
 for (unsigned i = 0; i < 256; i++) pal[i] = i * 74 + 1;

 static uint8_t ref_low[320 * 224], ref_high[320 * 224];
 static uint16_t ref_out_low[320 * 224], ref_out_high[320 * 224];
 static uint16_t nat_out_low[320 * 224], nat_out_high[320 * 224];

 for (int test = 0; test < 100; test++) {
  GVDP v;
  gvdp_init(&v);
  v.reg[1] = 0x44; // display enabled
  v.reg[12] = 1;  // H40 mode (320x224)
  v.reg[5] = 0x68; // SAT at 0xD000

  // Fill VRAM with pseudo-random pattern data
  for (unsigned i = 0; i < 65536; i++) {
   rng = rng * 1664525 + 1013904223;
   v.vram[i] = rng >> 24;
  }

  // Create a linked list of 1 to 20 sprites with varied properties
  int num_sprites = 1 + (test % 16);
  uint16_t sat_base = (uint16_t)((v.reg[5] & 0x7F) << 9);

  for (int i = 0; i < num_sprites; i++) {
   rng = rng * 1664525 + 1013904223;
   int sx = (int)(rng % 380) - 40; // test partially off-screen left and right
   rng = rng * 1664525 + 1013904223;
   int sy = (int)(rng % 280) - 30; // test partially off-screen top and bottom
   rng = rng * 1664525 + 1013904223;
   int hsz = 1 + (rng % 4), vsz = 1 + ((rng >> 4) % 4);
   int sz = ((hsz - 1) << 2) | (vsz - 1);
   int link = (i == num_sprites - 1) ? 0 : (i + 1);
   rng = rng * 1664525 + 1013904223;
   int pri = (rng >> 2) & 1;
   int pal_idx = (rng >> 4) & 3;
   int vf = (rng >> 6) & 1;
   int hf = (rng >> 7) & 1;
   int tile = (rng >> 8) & 0x3FF;

   gvdp_store_vram_word(&v, sat_base + i * 8, sy + 128);
   gvdp_store_vram_word(&v, sat_base + i * 8 + 2, (sz << 8) | link);
   gvdp_store_vram_word(&v, sat_base + i * 8 + 4, (pri << 15) | (pal_idx << 13) | (vf << 12) | (hf << 11) | tile);
   gvdp_store_vram_word(&v, sat_base + i * 8 + 6, sx + 128);
  }

  // 1. Reference: rasterize with x2_sprite_line
  for (int y = 0; y < 224; y++) {
   x2_sprite_line(&v, y, ref_low + y * 320, ref_high + y * 320);
  }
  for (int i = 0; i < 320 * 224; i++) {
   ref_out_low[i] = (ref_low[i] & 15) ? pal[ref_low[i]] : 0;
   ref_out_high[i] = (ref_high[i] & 15) ? pal[ref_high[i]] : 0;
  }

  // 2. Native: extract SAT sprites and emit tiles
  memset(nat_out_low, 0, sizeof(nat_out_low));
  memset(nat_out_high, 0, sizeof(nat_out_high));

  X2SpriteDesc sprs[80];
  int spr_count = x2_sprite_extract(&v, sprs, 80);

  // Draw in reverse SAT order so earlier sprites overwrite later sprites
  for (int i = spr_count - 1; i >= 0; i--) {
   if (sprs[i].pri == 0) {
    x2_sprite_emit_tiles(sprs[i], [&](const X2Tile& t){
     rasterize_tile(nat_out_low, v.vram, pal, t);
    });
   }
  }

  for (int i = spr_count - 1; i >= 0; i--) {
   if (sprs[i].pri == 1) {
    x2_sprite_emit_tiles(sprs[i], [&](const X2Tile& t){
     rasterize_tile(nat_out_high, v.vram, pal, t);
    });
   }
  }

  // Compare pixel by pixel
  for (int i = 0; i < 320 * 224; i++) {
   if (nat_out_low[i] != ref_out_low[i] || nat_out_high[i] != ref_out_high[i]) {
    fprintf(stderr, "TEST %d MISMATCH at (%d, %d):\n", test, i % 320, i / 320);
    fprintf(stderr, "  nat_low=%04x ref_low=%04x\n", nat_out_low[i], ref_out_low[i]);
    fprintf(stderr, "  nat_hi =%04x ref_hi =%04x\n", nat_out_high[i], ref_out_high[i]);
    fprintf(stderr, "Sprites on test %d (total %d):\n", test, num_sprites);
    for (int s = 0; s < spr_count; s++) {
     fprintf(stderr, "  spr %d: x=%d y=%d w=%d h=%d pri=%d pal=%d vf=%d hf=%d tile=%d\n",
      s, sprs[s].x, sprs[s].y, sprs[s].hsz*8, sprs[s].vsz*8, sprs[s].pri, sprs[s].pal, sprs[s].vf, sprs[s].hf, sprs[s].tile);
    }
    exit(1);
   }
  }
 }

 printf("ALL 100 SPRITE FIDELITY TESTS PASSED PIXEL-FOR-PIXEL!\n");
 return 0;
}
