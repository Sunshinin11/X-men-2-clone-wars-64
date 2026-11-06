#pragma once
#include "scene.hpp"
struct X2RectWords{uint32_t w[4];};
// Exactly the fixed-point packet used by libdragon's texture-rectangle helper.
// Geometry is already clipped by the scene walker; derivatives are +/- 1 texel.
static inline X2RectWords x2_rect_words(const X2Tile&t){
 static const uint32_t k_step[4] = { 0x04000400, 0xFC000400, 0x0400FC00, 0xFC00FC00 };
 unsigned pal = ((uint32_t)t.attr >> 13) & 3;
 int s = (t.attr & 2048) ? 7 - t.sx : t.sx;
 int row = (t.attr & 4096) ? 7 - t.sy : t.sy;
 int texy = ((t.attr & 31) << 3) + row;
 X2RectWords r = {{
  ((uint32_t)(t.x + t.w) << 14) | ((uint32_t)(t.y + t.h) << 2),
  (pal << 24) | ((uint32_t)t.x << 14) | ((uint32_t)t.y << 2),
  ((uint32_t)s << 21) | ((uint32_t)texy << 5),
  k_step[(t.attr >> 11) & 3]
 }};
 return r;
}
struct X2PageBatch{
 static constexpr unsigned CAPACITY=4096,END=65535;
 X2Tile tiles[CAPACITY];uint16_t next[CAPACITY],head[64],tail[64];unsigned count;
 void reset(){count=0;for(unsigned i=0;i<64;i++)head[i]=tail[i]=END;}
 void add(const X2Tile&t){unsigned p=(t.attr&2047)>>5,i=count++;tiles[i]=t;next[i]=END;if(head[p]==END)head[p]=i;else next[tail[p]]=i;tail[p]=i;}
 template<class Upload,class Draw> void flush(Upload upload,Draw draw){
  for(unsigned p=0;p<64;p++)if(head[p]!=END){upload(p);for(unsigned i=head[p];i!=END;i=next[i])draw(tiles[i]);}
  reset();
 }
};
struct X2ColData{
 uint32_t xw14;
 uint32_t x14;
 uint8_t sx;
};

struct X2RowData{
 uint32_t yh2;
 uint32_t y2;
 uint8_t sy;
};

struct X2BatchNode{
 uint16_t attr;
 uint16_t next;
 uint8_t c;
 uint8_t r;
 uint8_t last_col;
};

struct X2DualBatch{
 static constexpr unsigned CAPACITY=1200,END=65535;
 X2BatchNode nodes[CAPACITY];
 uint16_t head[2][64],tail[2][64];
 uint64_t active_mask[2];
 uint16_t count;

 X2ColData col_data[64];
 X2RowData row_data[64];
 uint8_t num_cols;
 uint8_t num_rows;

 bool merge_runs;
 void init(bool merge=false){
  merge_runs=merge;
  count=0;active_mask[0]=active_mask[1]=0;
  for(unsigned p=0;p<64;p++){head[0][p]=head[1][p]=tail[0][p]=tail[1][p]=END;}
  num_cols=num_rows=0;
 }

 void reset(){
  for(int pri=0; pri<2; pri++){
   uint64_t mask = active_mask[pri];
   while(mask){
    unsigned p = __builtin_ctzll(mask);
    head[pri][p] = END;
    tail[pri][p] = END;
    mask &= mask - 1;
   }
   active_mask[pri] = 0;
  }
  count = 0;
  num_cols = num_rows = 0;
 }

 inline void add(int pri, uint8_t c, uint8_t r, uint16_t attr){
  unsigned p = (attr & 2047) >> 5;
  uint16_t t = tail[pri][p];
  if(merge_runs && t != END){
   auto& previous = nodes[t];
   if(previous.r == r && previous.attr == attr && col_data[c].sx == 0 &&
      col_data[previous.last_col].xw14 == col_data[c].x14){
    previous.last_col = c;
    return;
   }
  }
  assert(count<CAPACITY);
  unsigned i = count++;
  nodes[i].attr = attr;
  nodes[i].next = END;
  nodes[i].c = c;
  nodes[i].r = r;
  nodes[i].last_col = c;
  if(t == END){
   head[pri][p] = i;
   active_mask[pri] |= (1ULL << p);
  } else {
   nodes[t].next = i;
  }
  tail[pri][p] = i;
 }

 void add_fallback(int pri, const X2Tile& t){
  if(num_cols < 64 && num_rows < 64 && count < CAPACITY){
   uint8_t c = num_cols++;
   uint8_t r = num_rows++;
   col_data[c] = { ((uint32_t)(t.x + t.w)) << 14, ((uint32_t)t.x) << 14, t.sx };
   row_data[r] = { ((uint32_t)(t.y + t.h)) << 2, ((uint32_t)t.y) << 2, t.sy };
   add(pri, c, r, t.attr);
  }
 }

 template<class Upload,class Draw> void flush_pri(int pri,Upload upload,Draw draw){
  uint64_t mask = active_mask[pri];
  while(mask){
   unsigned p = __builtin_ctzll(mask);
   upload(p);
   for(unsigned i=head[pri][p]; i!=END; i=nodes[i].next){
    const auto& node = nodes[i];
    X2ColData span = col_data[node.c];
    span.xw14 = col_data[node.last_col].xw14;
    draw(node, span, row_data[node.r]);
   }
   mask &= mask - 1;
  }
 }
};

struct X2ComplexBatch {
 // Allow tile spans split at every column-scroll and window boundary.
 static constexpr unsigned CAPACITY=224*64, END=65535;
 X2Tile tiles[CAPACITY]; uint16_t next[CAPACITY],head[2][64],tail[2][64]; unsigned count;
 void reset(){count=0;for(unsigned q=0;q<2;q++)for(unsigned p=0;p<64;p++)head[q][p]=tail[q][p]=END;}
 void add(unsigned pri,const X2Tile&t){
  assert(count<CAPACITY); unsigned i=count++,p=(t.attr&2047)>>5;
  tiles[i]=t;next[i]=END;if(head[pri][p]==END)head[pri][p]=i;else next[tail[pri][p]]=i;tail[pri][p]=i;
 }
 template<class Upload,class Draw> void flush(unsigned pri,Upload up,Draw draw){
  for(unsigned p=0;p<64;p++)if(head[pri][p]!=END){up(p);for(unsigned i=head[pri][p];i!=END;i=next[i])draw(tiles[i]);}
 }
};
