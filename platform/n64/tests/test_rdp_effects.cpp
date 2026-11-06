#include <libdragon.h>
#include "../operator_masks.hpp"
#include <cstdio>
extern "C" bool x2_rdp_draw(surface_t*,GVDP*,const uint16_t*);
int main(){
 static GVDP v;gvdp_init(&v);v.reg[1]=0x44;v.reg[12]=0x89;v.reg[2]=0x38;v.reg[4]=0;v.reg[5]=0x78;v.reg[13]=0x3d;v.reg[16]=0x11;
 for(unsigned y=0;y<8;y++)for(unsigned x=0;x<4;x++){v.vram[32+y*4+x]=0x22;v.vram[64+y*4+x]=0xef;v.vram[96+y*4+x]=0xfe;}
 for(unsigned i=0;i<64*32;i++)gvdp_store_vram_word(&v,i*2,1|((i&1)?0x8000:0));
 // Operator sprite covers both high and low priority background tiles.
 gvdp_store_vram_word(&v,0xf000,128);gvdp_store_vram_word(&v,0xf002,0x400);gvdp_store_vram_word(&v,0xf004,0x6002);gvdp_store_vram_word(&v,0xf006,128);
 uint16_t pal[256];for(unsigned b=0;b<3;b++)for(unsigned i=0;i<64;i++)pal[b*64+i]=((b*9+i)%32)<<11|((b*7+i)%32)<<6|((b*5+i)%32)<<1|1;
 surface_t fb=surface_alloc(FMT_RGBA16,320,240);
 for(unsigned mode=0;mode<10;mode++){
  v.reg[12]=mode?0x89:0x81;if(mode==2)gvdp_store_vram_word(&v,0xf004,0xe002);
  if(mode==3){
   pal[2]^=0x800;pal[66]^=0x800;pal[130]^=0x800;
   gvdp_store_vram_word(&v,0xf002,0x401);gvdp_store_vram_word(&v,0xf004,0x6002);
   for(unsigned y=0;y<8;y++)for(unsigned x=0;x<4;x++){v.vram[128+y*4+x]=0xfe;v.vram[160+y*4+x]=0xef;}
   gvdp_store_vram_word(&v,0xf008,128);gvdp_store_vram_word(&v,0xf00a,0x402);gvdp_store_vram_word(&v,0xf00c,0x6004);gvdp_store_vram_word(&v,0xf00e,128);
   gvdp_store_vram_word(&v,0xf010,128);gvdp_store_vram_word(&v,0xf012,0x400);gvdp_store_vram_word(&v,0xf014,0x2001);gvdp_store_vram_word(&v,0xf016,128);
  }
  if(mode==4)gvdp_store_vram_word(&v,0xf004,0x2001);
  if(mode==9){
   v.reg[11]=0;v.reg[17]=v.reg[18]=0;v.vsram[0]=v.vsram[1]=0;
   gvdp_store_vram_word(&v,0xf400,3);gvdp_store_vram_word(&v,0xf402,5);
   for(unsigned i=0;i<64*32;i++)gvdp_store_vram_word(&v,i*2,0x7801);
   for(unsigned i=0;i<32;i++)v.vram[32+i]=uint8_t((i*7)^0x5a);
  }
  if(mode>=5&&mode<9){
   v.reg[11]=mode==5?2:mode==6?3:7;
   for(unsigned y=0;y<224;y++){gvdp_store_vram_word(&v,0xf400+y*4,(y*3)&1023);gvdp_store_vram_word(&v,0xf402+y*4,(y*5)&1023);}
   for(unsigned i=0;i<40;i++)v.vsram[i]=(i*7)&1023;
   if(mode>=7){v.reg[17]=0x8a;v.reg[18]=0x8c;}
  }
  memset(x2_pattern_state,0,2048);assert(x2_rdp_draw(&fb,&v,pal));GVDP ref=v;uint8_t row[512];
  for(int y=0;y<224;y++){gvdp_render_scanline(&ref,y,row);for(int x=0;x<320;x++){
   uint16_t expected=pal[row[x]],actual=((uint16_t*)fb.buffer)[y*320+x];
   if((expected&65534)!=(actual&65534)){fprintf(stderr,"RDP effects mismatch mode=%u x=%d y=%d actual=%04x expected=%04x\n",mode,x,y,actual,expected);return 1;}
  }}
 }
 puts("RDP EFFECTS PASS production texture commands match reference normal/shadow/highlight colors and priorities");
}

