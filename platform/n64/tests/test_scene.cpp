#include <cstring>
#include <cassert>
#include <cstdint>
#include "../page_commands.hpp"
extern "C" void x2_scene_check(GVDP*,const uint16_t*,const uint16_t*);
int main(){
 unsigned rng=123;uint16_t pal[256];for(unsigned i=0;i<256;i++)pal[i]=i*74+1;
 static uint16_t frame[320*240];
 for(int test=0;test<80;test++){
  GVDP v;gvdp_init(&v);for(unsigned i=0;i<65536;i++){rng=rng*1664525+1013904223;v.vram[i]=rng>>24;}
  v.reg[1]=0x44;v.reg[2]=0x30;v.reg[3]=0x28;v.reg[4]=7;v.reg[5]=0x68;v.reg[7]=test&63;v.reg[12]=1;v.reg[13]=0x3c;
  v.reg[16]=(test%3==0?0:test%3==1?1:3)|((test%4==0?1:0)<<4);
  v.reg[11]=(test%3==0?0:test%3==1?2:3)|(test&1?4:0);v.reg[17]=test%4==0?0:((test&1?128:0)|10);v.reg[18]=test%4==0?0:((test&2?128:0)|12);
  for(int i=0;i<40;i++)v.vsram[i]=(test*13+i*7)&1023;
  for(int i=0;i<8;i++){unsigned a=0xd000+i*8;gvdp_store_vram_word(&v,a,128+i*21);gvdp_store_vram_word(&v,a+2,0x500|(i==7?0:i+1));gvdp_store_vram_word(&v,a+4,(i*32)|((i&3)<<13)|((i&1)?0x8000:0)|((i&2)?0x1800:0));gvdp_store_vram_word(&v,a+6,110+i*41);}
  assert(x2_scene_can(&v));GVDP ref=v;uint8_t row[512];for(int y=0;y<240;y++){int w=y<224?gvdp_render_scanline(&ref,y,row):0;for(int x=0;x<320;x++)frame[y*320+x]=x<w?pal[row[x]]:1;}
  x2_scene_check(&v,pal,frame);assert(v.sprite_collision==ref.sprite_collision&&v.sprite_overflow==ref.sprite_overflow&&v.spr_mask_carry==ref.spr_mask_carry&&!memcmp(v.spr_edge,ref.spr_edge,sizeof v.spr_edge));
 }
 static X2PageBatch batch;static bool seen[4353];unsigned page=0,drawn=0;batch.reset();
 auto upload=[&](unsigned p){page=p;};auto draw=[&](const X2Tile&t){assert(t.x>=0&&t.x<4353&&!seen[t.x]&&((t.attr&2047)>>5)==page);seen[t.x]=true;drawn++;};
 for(int i=0;i<4353;i++){if(batch.count==X2PageBatch::CAPACITY)batch.flush(upload,draw);X2Tile t={};t.x=i;t.attr=(i*73)&2047;batch.add(t);}batch.flush(upload,draw);assert(drawn==4353);}
