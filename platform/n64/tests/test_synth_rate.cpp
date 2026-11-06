#include <cassert>
#include <cstdio>
#include <stdint.h>
extern "C" {
#include "audio/ym2612.h"
#include "audio/sn76489.h"
void x2_ym_rate(unsigned);void x2_psg_rate(unsigned);unsigned x2_psg_active();
}
int main(){
 x2_ym_rate(12000);x2_psg_rate(12000);ym2612_init();psg_init();
 assert(ym2612_sample_rate()==12000&&psg_sample_rate()==12000);
 // Demand all PSG channels. Only two may be selected alongside six FM slots.
 psg_write(0x90);psg_write(0xb1);psg_write(0xd2);psg_write(0xf3);assert(x2_psg_active()==2);
 unsigned fm=0,psg=0;int16_t f[400],p[200];
 for(unsigned i=0;i<100;i++){ym2612_advance(53693);psg_advance(53693);fm+=ym2612_render(f,200);psg+=psg_render(p,200);}
 assert(fm==1199&&psg==1199);
 printf("SYNTH RATE PASS FM=%u PSG=%u samples in 100ms (fractional clock retained), maximum voices=6+2=8\n",fm,psg);
}
