#include "../native_pacing.hpp"
#include <cassert>
#include <cstdio>
int main(){
 bool late;
 assert(x2_next_present_vblank(10,10,true,late,1)==11&&!late);
 assert(x2_next_present_vblank(11,10,true,late,1)==12&&late);
 assert(x2_next_present_vblank(0xffffffffu,0xffffffffu,true,late,1)==0&&!late);
 assert(x2_next_present_vblank(10,0,false,late)==11&&!late);
 assert(x2_next_present_vblank(11,10,true,late)==12&&!late);
 assert(x2_next_present_vblank(12,10,true,late)==13&&late);
 assert(x2_next_present_vblank(20,10,true,late)==21&&late);
 assert(x2_next_present_vblank(0xffffffffu,0xfffffffeu,true,late)==0&&!late);
 assert(!x2_vblank_reached(0xffffffffu,0));assert(x2_vblank_reached(0,0));
 uint32_t last=0xfffff000u;
 for(unsigned f=0;f<100000;f++){
  uint32_t now=last+1,target=x2_next_present_vblank(now,last,true,late);
  assert(!late&&target-last==2&&!x2_vblank_reached(now,target));
  assert(x2_vblank_reached(++now,target));last=now;
 }
 puts("PACING PASS initial/normal/late deadlines, 32-bit wrap and 100000 two-VBlank submissions");
}
