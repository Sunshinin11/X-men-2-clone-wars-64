extern "C" {
#include "genesis_vdp.h"
}
#include <cassert>
#include <cstring>
#include <cstdio>
extern "C" {
void x2_video_init(GVDP*);void x2_video_command(GVDP*,uint16_t);void x2_video_upload(GVDP*,uint16_t);
uint16_t x2_video_read(GVDP*);
}
static uint16_t source(void*,uint32_t a){return uint16_t(a^(a>>9));}
int main(){
 static GVDP ref,native;gvdp_init(&ref);x2_video_init(&native);ref.bus_read=native.bus_read=source;
 auto command=[&](uint16_t w){gvdp_write_control(&ref,w);x2_video_command(&native,w);};
 auto data=[&](uint16_t w){gvdp_write_data(&ref,w);x2_video_upload(&native,w);};
 auto address=[&](unsigned a,unsigned code){command((a&0x3fff)|((code&3)<<14));command(((a>>14)&3)|((code&0x3c)<<2));};
 auto compare=[&](){assert(!memcmp(ref.vram,native.vram,sizeof ref.vram));assert(!memcmp(ref.sat,native.sat,sizeof ref.sat));assert(!memcmp(ref.cram,native.cram,sizeof ref.cram));assert(!memcmp(ref.vsram,native.vsram,sizeof ref.vsram));assert(ref.address==native.address&&ref.code==native.code);};
 command(0x8154);command(0x8c81);command(0x8578);
 unsigned rng=55;
 for(unsigned i=0;i<2000;i++){
  rng=rng*1664525+1013904223;
  unsigned a=rng&65535,mode=(rng>>16)%6;
  command(0x8f00|((rng>>24)&7));command(0x9300|(1+(rng&127)));command(0x9400);
  if(mode<3){address(a,mode==0?1:mode==1?3:5);for(int k=0;k<7;k++)data(rng+k);}
  else{command(0x9500|(rng&255));command(0x9600|((rng>>8)&255));command(0x9700|(mode==3?0:mode==4?128:192));address(a,0x21);if(mode==4)data(rng);}
  compare();address(a,0);assert(gvdp_read_data(&ref)==x2_video_read(&native));compare();
 }
 puts("NATIVE VIDEO PASS 2000 upload/read/copy/fill/transfer cases, tile/sprite/palette/scroll data match reference");
}
