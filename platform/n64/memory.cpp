#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
extern "C" {
static uint8_t arena_storage[0x20000] __attribute__((aligned(16)));
uint8_t *xm_arena=arena_storage,*xm_arena_ffff=arena_storage+0x10000;
uint8_t xm_tags[0x10000];
uint8_t g_rom[0x200000] __attribute__((aligned(16)));
}
namespace xm {
char g_fatal_msg[512];
[[noreturn]] void fatal(const char*fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(g_fatal_msg,sizeof(g_fatal_msg),fmt,ap);va_end(ap);fprintf(stderr,"[X2] %s\n",g_fatal_msg);abort();}
static uint32_t sp;
// Canonical big-endian RAM: preserve aliasing and unaligned/mixed-width accesses.
// Fixed widths avoid the byte-at-a-time loop on every game memory access.
uint32_t mem_load_slow(uint32_t off,int width){
 if(width<1||width>4||off+(unsigned)width>0x10000)fatal("RAM read %x/%d",off,width);
 typedef uint16_t U16 __attribute__((may_alias));
 typedef uint32_t U32 __attribute__((may_alias));
 const uint8_t*p=xm_arena+off;
 if(width==1)return p[0];
 if(width==2){if(off&1)return (uint32_t(p[0])<<8)|p[1];uint16_t v=*(const U16*)p;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
 v=__builtin_bswap16(v);
#endif
 return v;}
 if(width==4){if(off&3)return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];uint32_t v=*(const U32*)p;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
 v=__builtin_bswap32(v);
#endif
 return v;}
 return (uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|p[2];
}
void mem_store_slow(uint32_t off,int width,uint32_t v){
 if(width<1||width>4||off+(unsigned)width>0x10000)fatal("RAM write %x/%d",off,width);
 typedef uint16_t U16 __attribute__((may_alias));
 typedef uint32_t U32 __attribute__((may_alias));
 uint8_t*p=xm_arena+off;
 if(width==1){p[0]=(uint8_t)v;return;}
 if(width==2){if(off&1){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;return;}uint16_t value=(uint16_t)v;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
 value=__builtin_bswap16(value);
#endif
 *(U16*)p=value;return;}
 if(width==4){if(off&3){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;return;}
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
 v=__builtin_bswap32(v);
#endif
 *(U32*)p=v;return;}
 p[0]=(uint8_t)(v>>16);p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)v;
}
uint32_t frame_mark(){return sp;}
void frame_release(uint32_t mark){sp=mark;}
uint32_t frame_alloc(uint32_t bytes){sp-=(bytes+1)&~1u;return sp;}
}
extern "C" uint8_t xm_ram_peek8(uint32_t off){return xm_arena[off&0xffff];}
extern "C" void xm_ram_image(uint8_t*out){memcpy(out,xm_arena,0x10000);}
extern "C" void xm_native_mem_init(){memset(xm_arena,0,0x20000);xm::frame_release((uint32_t)g_rom[0]<<24|(uint32_t)g_rom[1]<<16|(uint32_t)g_rom[2]<<8|g_rom[3]);}
