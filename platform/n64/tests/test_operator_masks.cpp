#include "../native_sprites.hpp"
#include <cassert>
#include <cstdio>
extern "C" void gvdp_store_vram_word(GVDP*,uint16_t,uint16_t);
int main(){
 static GVDP v;gvdp_init(&v);v.reg[12]=0x89;v.reg[5]=0x78;
 // Near ordinary sprite hides the operator to its right only where opaque.
 for(unsigned i=0;i<2;i++){
  unsigned a=0xf000+i*8;gvdp_store_vram_word(&v,a,128);
  gvdp_store_vram_word(&v,a+2,i?0:1);
  gvdp_store_vram_word(&v,a+4,i?0x6002:1);
  gvdp_store_vram_word(&v,a+6,128);
 }
 for(unsigned y=0;y<8;y++){v.vram[32+y*4]=0x10;v.vram[64+y*4]=0xef;}
 static X2NativeSprites sprites;sprites.build(&v);auto&masks=sprites.operators;
 for(unsigned y=0;y<8;y++){
  assert(masks.mask[0][y*320]==0);assert(masks.mask[1][y*320]==0);
  assert(masks.mask[1][y*320+1]==255);assert(masks.mask[0][y*320+1]==0);
 }
 assert(!masks.any(0)&&masks.any(1));
 puts("OPERATOR MASK PASS first-sprite ownership, transparent pixels, highlight/shadow separation");
}
