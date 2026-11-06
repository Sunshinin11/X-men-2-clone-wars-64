#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define surface_t X2RdpTestSurface
enum{FMT_CI4,FMT_RGBA16,FMT_IA8,TILE0=0,TILE1=1,FILTER_POINT=0,DITHER_NONE_NONE=0,TLUT_NONE=0,TLUT_RGBA16=1};
typedef int rdpq_tile_t;
struct surface_t{void*buffer;int format,width,height,stride;};
constexpr float REPEAT_INFINITE=2048;
struct rdpq_texparms_t{unsigned palette=0;struct {float repeats=0;} s;};
inline surface_t surface_make(void*p,int f,int w,int h,int stride){return{p,f,w,h,stride};}
inline surface_t surface_alloc(int f,int w,int h){return surface_make(calloc(w*h,2),f,w,h,w*2);}
inline surface_t surface_make_sub(surface_t*p,int x,int y,int w,int h){return surface_make((uint8_t*)p->buffer+y*p->stride+x*(p->format==FMT_RGBA16?2:1),p->format,w,h,p->stride);}
inline unsigned RGBA32(unsigned r,unsigned g,unsigned b,unsigned a){return r<<24|g<<16|b<<8|a;}
namespace fake_rdp{
inline int cx0=0,cy0=0,cx1=320,cy1=240;
inline surface_t*target;inline surface_t tex[8];inline unsigned palette[8];inline bool wrap_s[8];
inline uint16_t tlut[256],fill;inline unsigned combine=1;inline bool alpha_test;
inline uint16_t color(unsigned t,int x,int y){
 auto&s=tex[t];if(wrap_s[t])x=(x%s.width+s.width)%s.width;assert(x>=0&&x<s.width&&y>=0&&y<s.height);
 auto*p=(uint8_t*)s.buffer+y*s.stride;
 if(s.format==FMT_RGBA16)return ((uint16_t*)p)[x];
 if(s.format==FMT_IA8)return p[x]&15?0xffff:0;
 unsigned n=(p[x/2]>>((x&1)?0:4))&15;return tlut[palette[t]*16+n];
}
inline void rect(unsigned t,int x0,int y0,int x1,int y1,int sx,int sy,int dx,int dy){
 for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
  if(x<cx0||y<cy0||x>=cx1||y>=cy1||x<0||y<0||x>=target->width||y>=target->height)continue;
  int s=sx+(x-x0)*dx,v=sy+(y-y0)*dy;
  uint16_t c=color(t,s,v);
  if(combine==2&&!(color(t+1,s,v)&1))continue;
  if(alpha_test&&!(c&1))continue;
  ((uint16_t*)((uint8_t*)target->buffer+y*target->stride))[x]=c;
 }
}
}
inline void rdpq_attach(surface_t*s,void*){fake_rdp::target=s;fake_rdp::cx0=fake_rdp::cy0=0;fake_rdp::cx1=s->width;fake_rdp::cy1=s->height;}
inline void rdpq_set_scissor(int x0,int y0,int x1,int y1){fake_rdp::cx0=x0;fake_rdp::cy0=y0;fake_rdp::cx1=x1;fake_rdp::cy1=y1;}
inline void rdpq_detach_wait(){}
inline void rdpq_set_mode_fill(unsigned c){fake_rdp::fill=((c>>27)&31)<<11|((c>>19)&31)<<6|((c>>11)&31)<<1|1;}
inline void rdpq_fill_rectangle(int x0,int y0,int x1,int y1){auto&s=*fake_rdp::target;for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)if(x>=fake_rdp::cx0&&x<fake_rdp::cx1&&y>=fake_rdp::cy0&&y<fake_rdp::cy1)((uint16_t*)((uint8_t*)s.buffer+y*s.stride))[x]=fake_rdp::fill;}
inline void rdpq_set_mode_standard(){fake_rdp::combine=1;}
inline void rdpq_mode_filter(int){}inline void rdpq_mode_dithering(int){}inline void rdpq_mode_tlut(int){}
inline void rdpq_mode_alphacompare(int a){fake_rdp::alpha_test=a!=0;}
inline void rdpq_mode_combiner(unsigned c){fake_rdp::combine=c;}
#define RDPQ_COMBINER_TEX 1
#define RDPQ_COMBINER2(...) 2
inline void rdpq_tex_upload_tlut(const uint16_t*p,int start,int n){memcpy(fake_rdp::tlut+start,p,n*2);}
inline void rdpq_tex_multi_begin(){}inline void rdpq_tex_multi_end(){}
inline void rdpq_tex_upload(unsigned t,const surface_t*s,const rdpq_texparms_t*p){fake_rdp::tex[t]=*s;fake_rdp::palette[t]=p?p->palette:0;fake_rdp::wrap_s[t]=p&&p->s.repeats==REPEAT_INFINITE;}
inline void rdpq_tex_reuse(unsigned t,const rdpq_texparms_t*p){fake_rdp::tex[t]=fake_rdp::tex[0];fake_rdp::palette[t]=p?p->palette:0;fake_rdp::wrap_s[t]=p&&p->s.repeats==REPEAT_INFINITE;}
inline void rdpq_texture_rectangle(unsigned t,int x0,int y0,int x1,int y1,int sx,int sy){fake_rdp::rect(t,x0,y0,x1,y1,sx,sy,1,1);}
inline void data_cache_hit_writeback(const void*,size_t){}
inline unsigned TICKS_READ(){static unsigned n;return ++n;}
#define TICKS_TO_US(t) (t)
#define debugf(...) ((void)0)
extern "C" inline void __rdpq_texture_rectangle(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
 unsigned t=(b>>24)&7;int x0=((b>>12)&4095)/4,y0=(b&4095)/4,x1=((a>>12)&4095)/4,y1=(a&4095)/4;
 fake_rdp::rect(t,x0,y0,x1,y1,int16_t(c>>16)/32,int16_t(c)/32,int16_t(d>>16)/1024,int16_t(d)/1024);
}
