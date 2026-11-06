#pragma once
#include <stdint.h>
#include "scene.hpp"

struct X2SpriteDesc {
 int16_t x, y;
 uint8_t hsz, vsz;   // dimensions in 8x8 cells (1..4)
 uint8_t pal;        // palette index (0..3)
 uint8_t pri;        // priority (0 = low, 1 = high)
 uint8_t hf;         // horizontal flip (0 or 1)
 uint8_t vf;         // vertical flip (0 or 1)
 uint16_t tile;      // starting tile index in VRAM (0..2047)
};

// Traverses the SAT linked list and extracts on-screen sprites in SAT order.
static inline int x2_sprite_extract(const GVDP* v, X2SpriteDesc* list, int max_out) {
 int max_sprites = (v->reg[12] & 1) ? 80 : 64;
 int link = 0, count = 0;
 uint16_t sat_base = (uint16_t)((v->reg[5] & 0x7F) << 9);
 int spr_ovr = 0, masked = 0;

 for (int n = 0; n < max_sprites; n++) {
  uint16_t c = (uint16_t)(link * 8);
  int y = (((v->sat[c] << 8) | v->sat[c + 1]) & 0x1FF) - 128;
  uint8_t sz = v->sat[(c + 2) & 0x3FF];
  int hsz = ((sz >> 2) & 3) + 1;
  int vsz = (sz & 3) + 1;
  int width = hsz * 8;
  int height = vsz * 8;

  uint16_t attr = x2_word(v, sat_base + link * 8 + 4);
  int xword = x2_word(v, sat_base + link * 8 + 6) & 0x3FF;
  int x = xword - 128;

  // Mega Drive sprite masking: an x==0 sprite (xword & 0x1FF == 0) after a non-zero one masks all subsequent
  if (xword & 0x1FF) spr_ovr = 1;
  else if (spr_ovr) masked = 1;

  int next = v->sat[(c + 3) & 0x3FF] & 0x7F;

  if (!masked && (x + width > 0) && (x < 320) && (y + height > 0) && (y < 224)) {
   if (count < max_out) {
    list[count++] = {
     (int16_t)x, (int16_t)y,
     (uint8_t)hsz, (uint8_t)vsz,
     (uint8_t)((attr >> 13) & 3),
     (uint8_t)((attr >> 15) & 1),
     (uint8_t)((attr >> 11) & 1),
     (uint8_t)((attr >> 12) & 1),
     (uint16_t)(attr & 0x07FF)
    };
   }
  }

  if (next == 0 || next >= max_sprites) break;
  link = next;
 }
 return count;
}

// Emits individual 8x8 cell tiles for a given sprite, clipped to 320x224.
template<class Emit>
static inline void x2_sprite_emit_tiles(const X2SpriteDesc& spr, Emit emit) {
 int hsz = spr.hsz, vsz = spr.vsz;
 for (int cx = 0; cx < hsz; cx++) {
  int cell_x = spr.x + cx * 8;
  int clip_x0 = cell_x < 0 ? 0 : cell_x;
  int clip_x1 = (cell_x + 8 > 320) ? 320 : (cell_x + 8);
  if (clip_x1 <= clip_x0) continue;
  int w = clip_x1 - clip_x0;
  int sx = clip_x0 - cell_x;

  int eff_cx = spr.hf ? (hsz - 1 - cx) : cx;

  for (int cy = 0; cy < vsz; cy++) {
   int cell_y = spr.y + cy * 8;
   int clip_y0 = cell_y < 0 ? 0 : cell_y;
   int clip_y1 = (cell_y + 8 > 224) ? 224 : (cell_y + 8);
   if (clip_y1 <= clip_y0) continue;
   int h = clip_y1 - clip_y0;
   int sy = clip_y0 - cell_y;

   int eff_cy = spr.vf ? (vsz - 1 - cy) : cy;
   uint16_t tileno = (spr.tile + eff_cx * vsz + eff_cy) & 0x7FF;

   uint16_t attr = (spr.pri << 15) | (spr.pal << 13) | (spr.vf ? 4096 : 0) | (spr.hf ? 2048 : 0) | tileno;
   emit({(int16_t)clip_x0, (int16_t)clip_y0, (uint8_t)w, (uint8_t)h, (uint8_t)sx, (uint8_t)sy, attr});
  }
 }
}

