#include "video_native.h"
#include <string.h>

// Per-pattern blank cache for the renderer: 0 unknown, 1 all transparent, 2 drawn.
// Every VRAM store below forgets the pattern it touches.
uint8_t x2_pattern_state[2048];
uint64_t x2_video_dirty_pages;
void gvdp_init(GVDP*v){x2_video_dirty_pages=~(uint64_t)0;memset(v,0,sizeof(*v));v->reg[15]=2;memset(x2_pattern_state,0,sizeof x2_pattern_state);}
int gvdp_screen_width(const GVDP*v){return(v->reg[12]&1)?320:256;}
int gvdp_screen_height(const GVDP*v){return(v->reg[1]&8)?240:224;}
int gvdp_interlace_double(const GVDP*v){return((v->reg[12]>>1)&3)==3;}
int gvdp_output_height(const GVDP*v){return gvdp_screen_height(v)<<gvdp_interlace_double(v);}
int gvdp_display_enabled(const GVDP*v){return(v->reg[1]&64)!=0;}
void gvdp_sat_byte(GVDP*v,uint16_t a,uint8_t value){
 unsigned mask=(v->reg[12]&1)?0xfc00:0xfe00;
 if((a&mask)==((v->reg[5]<<9)&mask))v->sat[a&~mask]=value;
}
void gvdp_store_vram_word(GVDP*v,uint16_t a,uint16_t value){
 unsigned p=a&0xfffe;x2_video_dirty_pages|=(uint64_t)1<<(p>>10);x2_pattern_state[p>>5]=0;v->vram[p]=(a&1)?value:value>>8;v->vram[p+1]=(a&1)?value>>8:value;
 gvdp_sat_byte(v,p,v->vram[p]);gvdp_sat_byte(v,p+1,v->vram[p+1]);
}
void gvdp_store_cram(GVDP*v,uint16_t a,uint16_t value){
 unsigned p=(a>>1)&63;v->cram[p]=value&0xeee;
 if(v->colour_updated)v->colour_updated(v->colour_updated_user,p,v->cram[p]);
}
void gvdp_store_vsram(GVDP*v,uint16_t a,uint16_t value){unsigned p=a>>1;if(p<40)v->vsram[p]=value&0x7ff;}
static void upload_word(GVDP*v,uint16_t word){
 switch(v->code&15){case 3:gvdp_store_cram(v,v->address,word);break;case 5:gvdp_store_vsram(v,v->address,word);break;default:gvdp_store_vram_word(v,v->address,word);break;}
 v->address+=v->reg[15];
}
static unsigned transfer_length(const GVDP*v){unsigned n=v->reg[19]|v->reg[20]<<8;return n?n:65536;}
void gvdp_write_control(GVDP*v,uint16_t word){
 if(!v->control_pending){
  if((word&0xc000)==0x8000){unsigned r=(word>>8)&31;if(r<24)v->reg[r]=word;v->code=0;return;}
  v->address=(v->address&0xc000)|(word&0x3fff);v->code=(v->code&0x3c)|(word>>14);v->control_pending=1;return;
 }
 v->address=(v->address&0x3fff)|((word&3)<<14);v->code=(v->code&3)|((word>>2)&0x3c);v->control_pending=0;
 if(!(v->code&32)||!(v->reg[1]&16))return;
 unsigned n=transfer_length(v),mode=v->reg[23]&0xc0;
 if(mode<128){
  if(!v->bus_read)return;
  unsigned src=(v->reg[21]|v->reg[22]<<8|(v->reg[23]&127)<<16)<<1;
  while(n--){upload_word(v,v->bus_read(v->bus_read_user,src));src=(src&0xfe0000)|((src+2)&0x1ffff);}
 }else if(mode==128)v->dma_fill_pending=1;
 else{
  uint16_t src=v->reg[21]|v->reg[22]<<8;
  while(n--){uint8_t b=v->vram[src++];x2_video_dirty_pages|=(uint64_t)1<<(v->address>>10);x2_pattern_state[v->address>>5]=0;v->vram[v->address]=b;gvdp_sat_byte(v,v->address,b);v->address+=v->reg[15];}
 }
}
void gvdp_write_data(GVDP*v,uint16_t word){
 v->control_pending=0;
 if(!v->dma_fill_pending){upload_word(v,word);return;}
 gvdp_store_vram_word(v,v->address,word);v->address+=v->reg[15];
 unsigned n=transfer_length(v);while(n--){uint16_t a=v->address^1;x2_video_dirty_pages|=(uint64_t)1<<(a>>10);x2_pattern_state[a>>5]=0;v->vram[a]=word>>8;gvdp_sat_byte(v,a,word>>8);v->address+=v->reg[15];}
 v->dma_fill_pending=0;
}
uint16_t gvdp_read_data(GVDP*v){
 v->control_pending=0;unsigned a=v->address;uint16_t word;
 if((v->code&15)==8)word=v->cram[(a>>1)&63];
 else if((v->code&15)==4)word=(a>>1)<40?v->vsram[a>>1]:0;
 else{a&=0xfffe;word=(v->vram[a]<<8)|v->vram[a+1];}
 v->address+=v->reg[15];return word;
}
uint16_t gvdp_read_control(GVDP*v){
 // Uploads are synchronous native memory operations. No device busy-waiting.
 v->control_pending=0;v->vint_pending=0;
 return 0x3600|(v->in_vblank?8:0);
}
