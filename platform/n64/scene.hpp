#pragma once
#include <stdint.h>
#include <assert.h>
extern "C" {
#include "genesis_vdp.h"
void x2_sprite_line(GVDP*,int,uint8_t*,uint8_t*);
}
#ifndef X2_PLANE_CMD_CACHE
#define X2_PLANE_CMD_CACHE 1
#endif
extern "C" uint8_t x2_pattern_state[2048];
// A pattern whose 64 pixels are all colour 0 draws nothing (index 0 is
// transparent in every material), so its rectangle is skipped.
static inline bool x2_tile_blank(const GVDP*v,unsigned tile){
 tile&=2047;uint8_t s=x2_pattern_state[tile];
 if(!s){
#if X2_PLANE_CMD_CACHE
  const uint32_t*p=(const uint32_t*)(v->vram+tile*32);
  uint32_t o=p[0]|p[1]|p[2]|p[3]|p[4]|p[5]|p[6]|p[7];
  s=o?2:1;
#else
  const uint8_t*p=v->vram+tile*32;uint8_t o=0;for(int i=0;i<32;i++)o|=p[i];s=o?2:1;
#endif
  x2_pattern_state[tile]=s;
 }
 return s==1;
}
struct X2Tile {int16_t x,y;uint8_t w,h,sx,sy;uint16_t attr;};
static inline int x2_tiles(unsigned c){return (c&3)==1?64:(c&3)==3?128:32;}
static inline uint16_t x2_word(const GVDP*v,unsigned a){
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
 return *(const uint16_t*)(v->vram + (a & 0xFFFE));
#else
 return (v->vram[a&65535]<<8)|v->vram[(a+1)&65535];
#endif
}
static inline bool x2_scene_can(const GVDP*v){return gvdp_display_enabled(v)&&gvdp_screen_width(v)==320&&gvdp_output_height(v)==224&&!gvdp_interlace_double(v);}

// The compact row/column batch is valid only for a rectangular scroll grid.
static inline bool x2_window_visible(const GVDP*v){
 int x=(v->reg[17]&31)*16,y=(v->reg[18]&31)*8;
 return ((v->reg[17]&128)?x<320:x>0)||((v->reg[18]&128)?y<224:y>0);
}
static inline bool x2_plane_grid(const GVDP*v,int plane){
 return !(v->reg[11]&7) && (plane || !x2_window_visible(v));
}

template<class Emit0, class Emit1>
static void x2_plane_dual(const GVDP*v,int plane,Emit0 emit0,Emit1 emit1){
 int wt = x2_tiles(v->reg[16]), ht = x2_tiles(v->reg[16]>>4);
 int ht_mask = ht - 1;
 int tw_shift = (wt == 128) ? 7 : (wt == 64) ? 6 : 5;
 unsigned base = plane ? ((v->reg[4]&7)<<13) : ((v->reg[2]&56)<<10);
 unsigned win = (v->reg[3]&62)<<10;
 unsigned hbase = (v->reg[13]&63)<<10;
 int hmode = v->reg[11] & 3;
 bool col_scroll = (v->reg[11] & 4) != 0;

 // Fast path: full-screen H-scroll, full-screen V-scroll, and no window on Plane A
 bool has_window = (!plane) && x2_window_visible(v);
 if(!col_scroll && hmode == 0 && !has_window){
  int hs = x2_word(v, hbase + plane * 2) & 1023;
  int vs = v->vsram[plane] & 1023;
  int wt_mask = wt - 1;

  struct X2Col { int16_t x; uint8_t w, sx, col_idx; };
  X2Col cols[42];
  int num_cols = 0;
  for(int x=0; x<320;){
   int sx = x - hs;
   int w = 8 - (sx & 7);
   if(w > 320 - x) w = 320 - x;
   cols[num_cols++] = { (int16_t)x, (uint8_t)w, (uint8_t)(sx & 7), (uint8_t)(((sx >> 3) & wt_mask)) };
   x += w;
  }

  const uint16_t* vram16 = (const uint16_t*)v->vram;
  for(int y=0; y<224;){
   int sy = y + vs;
   int h = 8 - (sy & 7);
   if(h > 224 - y) h = 224 - y;
   int row_offset = (((sy >> 3) & ht_mask) << tw_shift);
   const uint16_t* row_vram = vram16 + (base >> 1) + row_offset;
   uint8_t sy7 = (uint8_t)(sy & 7);
   uint8_t uh = (uint8_t)h;
   int16_t iy = (int16_t)y;

   for(int c=0; c<num_cols; c++){
    uint16_t attr = row_vram[cols[c].col_idx];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
    attr = (attr >> 8) | (attr << 8);
#endif
    if(attr >> 15){
     emit1({cols[c].x, iy, cols[c].w, uh, cols[c].sx, sy7, attr});
    } else {
     emit0({cols[c].x, iy, cols[c].w, uh, cols[c].sx, sy7, attr});
    }
   }
   y += h;
  }
  return;
 }

 // General line-by-line fallback for complex scroll / window modes
 X2Tile buf0[2][160], buf1[2][160];
 X2Tile* old0 = buf0[0], *next0 = buf0[1];
 X2Tile* old1 = buf1[0], *next1 = buf1[1];
 int count0 = 0, count1 = 0;
 int vs_const = col_scroll ? 0 : (v->vsram[plane] & 1023);
 int wx = (v->reg[17] & 31) * 16;
 bool win_x_right = (v->reg[17] & 128) != 0;
 bool win_y_down = (v->reg[18] & 128) != 0;
 int wy = (v->reg[18] & 31) * 8;

 for(int y=0; y<224; y++){
  int n0 = 0, n1 = 0;
  int hi = (hmode == 0) ? 0 : (hmode == 2) ? (y & ~7) : y;
  int hs = x2_word(v, hbase + hi * 4 + plane * 2) & 1023;
  bool win_y = !plane && (win_y_down ? (y >= wy) : (y < wy));

  for(int x=0; x<320;){
   int vs = col_scroll ? (v->vsram[((x>>4)*2) + plane] & 1023) : vs_const;
   bool window = !plane && (win_y || (win_x_right ? (x >= wx) : (x < wx)));
   int sx = window ? x : (x - hs);
   int sy = window ? y : (y + vs);
   int tw = window ? 64 : wt;
   int tw_s = window ? 6 : tw_shift;
   unsigned table = window ? win : base;
   unsigned addr = table + ((((sy >> 3) & (window ? 63 : ht_mask)) << tw_s) + ((sx >> 3) & (tw - 1))) * 2;
   uint16_t attr = x2_word(v, addr);

   int width = 8 - (sx & 7);
   if(col_scroll && width > 16 - (x & 15)) width = 16 - (x & 15);
   if(!plane && x < wx && width > wx - x) width = wx - x;
   if(width > 320 - x) width = 320 - x;

   if(attr >> 15) {
    next1[n1++] = {(int16_t)x, (int16_t)y, (uint8_t)width, (uint8_t)1, (uint8_t)(sx & 7), (uint8_t)(sy & 7), attr};
   } else {
    next0[n0++] = {(int16_t)x, (int16_t)y, (uint8_t)width, (uint8_t)1, (uint8_t)(sx & 7), (uint8_t)(sy & 7), attr};
   }
   x += width;
  }

  // Merge priority 0
  int i = 0;
  for(int j=0; j<n0; j++){
   while(i < count0 && old0[i].x < next0[j].x) emit0(old0[i++]);
   if(i < count0 && old0[i].x == next0[j].x){
    const X2Tile& a = old0[i++];
    X2Tile& b = next0[j];
    if(a.attr == b.attr && a.w == b.w && a.sx == b.sx && a.sy + a.h == b.sy && a.y + a.h == b.y){
     b.y = a.y; b.h = a.h + 1; b.sy = a.sy;
    } else emit0(a);
   }
  }
  while(i < count0) emit0(old0[i++]);
  X2Tile* swap0 = old0; old0 = next0; next0 = swap0; count0 = n0;

  // Merge priority 1
  i = 0;
  for(int j=0; j<n1; j++){
   while(i < count1 && old1[i].x < next1[j].x) emit1(old1[i++]);
   if(i < count1 && old1[i].x == next1[j].x){
    const X2Tile& a = old1[i++];
    X2Tile& b = next1[j];
    if(a.attr == b.attr && a.w == b.w && a.sx == b.sx && a.sy + a.h == b.sy && a.y + a.h == b.y){
     b.y = a.y; b.h = a.h + 1; b.sy = a.sy;
    } else emit1(a);
   }
  }
  while(i < count1) emit1(old1[i++]);
  X2Tile* swap1 = old1; old1 = next1; next1 = swap1; count1 = n1;
 }

 for(int i=0; i<count0; i++) emit0(old0[i]);
 for(int i=0; i<count1; i++) emit1(old1[i]);
}

template<class Emit>
static void x2_plane(const GVDP*v,int plane,int priority,Emit emit){
 if(priority == 0){
  auto noop = [](const X2Tile&){};
  x2_plane_dual(v, plane, emit, noop);
 } else {
  auto noop = [](const X2Tile&){};
  x2_plane_dual(v, plane, noop, emit);
 }
}

template<class Batch>
static void x2_plane_batch(const GVDP*v,int plane,Batch&batch,uint64_t* dependencies=nullptr){
 assert(x2_plane_grid(v,plane));
 int wt = x2_tiles(v->reg[16]), ht = x2_tiles(v->reg[16]>>4);
 int ht_mask = ht - 1;
 int tw_shift = (wt == 128) ? 7 : (wt == 64) ? 6 : 5;
 unsigned base = plane ? ((v->reg[4]&7)<<13) : ((v->reg[2]&56)<<10);
 unsigned hbase = (v->reg[13]&63)<<10;
 int hmode = v->reg[11] & 3;
 bool col_scroll = (v->reg[11] & 4) != 0;

 bool has_window = (!plane) && x2_window_visible(v);
 if(!col_scroll && hmode == 0 && !has_window){
  int hs = x2_word(v, hbase + plane * 2) & 1023;
  int vs = v->vsram[plane] & 1023;
  int wt_mask = wt - 1;

  uint8_t cols[42];
  int num_cols = 0;
  for(int x=0; x<320;){
   int sx = x - hs;
   int w = 8 - (sx & 7);
   if(w > 320 - x) w = 320 - x;
   cols[num_cols] = (uint8_t)(((sx >> 3) & wt_mask));
   batch.col_data[num_cols] = {
    ((uint32_t)(x + w)) << 14,
    ((uint32_t)x) << 14,
    (uint8_t)(sx & 7)
   };
   num_cols++;
   x += w;
  }
  batch.num_cols = num_cols;

  int num_rows = 0;
  uint16_t row_offsets[32];
  for(int y=0; y<224;){
   int sy = y + vs;
   int h = 8 - (sy & 7);
   if(h > 224 - y) h = 224 - y;
   row_offsets[num_rows] = (((sy >> 3) & ht_mask) << tw_shift);
   batch.row_data[num_rows] = {
    ((uint32_t)(y + h)) << 2,
    ((uint32_t)y) << 2,
    (uint8_t)(sy & 7)
   };
   num_rows++;
   y += h;
  }
  batch.num_rows = num_rows;

#if X2_PLANE_CMD_CACHE
  uint64_t dep = 0;
  for(int r=0;r<num_rows;r++){
   unsigned row_base = (base + 2 * row_offsets[r]) & 65535;
   dep |= (1ULL << (row_base >> 10));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
   const uint16_t* row_vram = (const uint16_t*)(v->vram + row_base);
#endif
   for(int c=0;c<num_cols;c++){
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint16_t attr = row_vram[cols[c]];
#else
    uint16_t attr = x2_word(v, row_base + 2 * cols[c]);
#endif
    dep |= (1ULL << ((attr & 2047) >> 5));
    if(!x2_tile_blank(v,attr))batch.add(attr>>15,c,r,attr);
   }
  }
  if(dependencies)*dependencies |= dep;
#else
  for(int r=0;r<num_rows;r++){
   for(int c=0;c<num_cols;c++){
    unsigned address=base+2*(row_offsets[r]+cols[c]);
    uint16_t attr=x2_word(v,address);
    if(dependencies)*dependencies|=(1ULL<<((address&65535)>>10))|(1ULL<<((attr&2047)>>5));
    if(!x2_tile_blank(v,attr))batch.add(attr>>15,c,r,attr);
   }
  }
#endif
  return;
 }

 x2_plane_dual(v, plane,
  [&](const X2Tile& t){ if(!x2_tile_blank(v,t.attr))batch.add_fallback(0, t); },
  [&](const X2Tile& t){ if(!x2_tile_blank(v,t.attr))batch.add_fallback(1, t); });
}
