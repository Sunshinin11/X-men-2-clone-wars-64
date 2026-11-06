#include <stdint.h>
#include <cstdio>
#include <cassert>
#include <chrono>
#include <cstring>
extern "C" {extern uint8_t*xm_arena;}
namespace xm {uint32_t mem_load_slow(uint32_t,int);void mem_store_slow(uint32_t,int,uint32_t);}
__attribute__((noinline)) uint32_t old_load(uint32_t off,int w){uint32_t v=0;for(int i=0;i<w;i++)v=v<<8|xm_arena[off+i];return v;}
int main(){
 uint8_t expected[65536];uint32_t rng=1;
 for(auto&b:expected){rng=rng*1664525+1013904223;b=rng>>24;}memcpy(xm_arena,expected,sizeof expected);
 for(unsigned w=1;w<=4;w++)for(unsigned off=0;off<=65536-w;off++)assert(xm::mem_load_slow(off,w)==old_load(off,w));
 for(unsigned n=0;n<500000;n++){rng=rng*1664525+1013904223;unsigned w=(rng>>24)%4+1,off=rng%(65537-w);uint32_t value=rng*31;
  xm::mem_store_slow(off,w,value);for(unsigned i=w;i--;){expected[off+i]=value;value>>=8;}
  assert(!memcmp(xm_arena+off,expected+off,w));assert(xm::mem_load_slow(off,w)==old_load(off,w));}
 assert(!memcmp(xm_arena,expected,sizeof expected));
 volatile uint32_t sink=0;
 auto bench=[&](bool old){auto begin=std::chrono::steady_clock::now();for(unsigned i=0;i<1000000;i++)sink=old?old_load(i&65531,4):xm::mem_load_slow(i&65531,4);return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();};
 auto before=bench(true),after=bench(false);
 printf("RAM PASS all offsets widths 1..4, 500000 mixed unaligned writes; host microbench old_us=%lld new_us=%lld (not N64 FPS)\n",(long long)before,(long long)after);
}
