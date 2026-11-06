// Original SN76489 register-controlled tone/noise, evaluated directly at 12 kHz.
#include <stdint.h>
#include <string.h>
#include <stddef.h>
static const int16_t volume[16]={8191,6506,5168,4105,3261,2590,2057,1634,1298,1031,819,650,516,410,326,0};
static uint16_t period[3],lfsr;
static uint8_t atten[4],latch,noise,mask;
static uint32_t phase[3],step[3];static uint64_t noise_phase,accum;static unsigned rate=12000,wr,rd;
static int16_t samples[8192];
static void tune(unsigned c){unsigned p=period[c]?period[c]:1;step[c]=(uint32_t)((223721ULL<<32)/(2ULL*p*rate));}
void x2_psg_rate(unsigned r){rate=r;for(unsigned c=0;c<3;c++)tune(c);}
unsigned x2_psg_active(void){unsigned n=0;for(int c=0;c<4;c++)if((mask&(1<<c))&&atten[c]<15)n++;return n;}
static void select_voices(void){ // Six FM/DAC slots plus at most TWO PSG slots = eight.
 mask=0;
 for(int k=0;k<2;k++){int best=-1,bestscore=-1;for(int c=0;c<4;c++)if(!(mask&(1<<c))&&atten[c]<15){int score=15-atten[c]+(c==3?16:0);if(score>bestscore){best=c;bestscore=score;}}
  if(best>=0)mask|=1<<best;
 }
}
void psg_init(void){memset(phase,0,sizeof phase);for(int c=0;c<3;c++)period[c]=1;for(unsigned c=0;c<3;c++)tune(c);memset(atten,15,4);latch=noise=mask=0;lfsr=0x8000;noise_phase=accum=0;wr=rd=0;}
void psg_write(uint8_t v){if(v&128)latch=(v>>4)&7;unsigned c=latch>>1;
 if(latch&1)atten[c]=v&15;
 else if(c<3){if(v&128)period[c]=(period[c]&0x3f0)|(v&15);else period[c]=(period[c]&15)|((v&63)<<4);tune(c);}
 else{noise=v&7;lfsr=0x8000;}
 select_voices();
}
static int16_t sample(void){int mix=0;
 for(int c=0;c<3;c++){phase[c]+=step[c];if(mask&(1<<c))mix+=(phase[c]&0x80000000)?volume[atten[c]]:-volume[atten[c]];}
 unsigned p=(noise&3)==3?(period[2]?period[2]:1):16u<<(noise&3);
 noise_phase+=223721;while(noise_phase>=2u*p*rate){noise_phase-=2u*p*rate;unsigned feedback=(noise&4)?((lfsr^(lfsr>>3))&1):(lfsr&1);lfsr=(lfsr>>1)|(feedback<<15);}
 if(mask&8)mix+=(lfsr&1)?volume[atten[3]]:-volume[atten[3]];
 return (int16_t)mix;
}
void psg_advance(uint32_t delta){accum+=(uint64_t)delta*rate;while(accum>=53693175){accum-=53693175;int16_t v=sample();if(wr<8192)samples[wr++]=v;}}
size_t psg_render(int16_t*out,size_t n){unsigned available=wr-rd;if(n>available)n=available;memcpy(out,samples+rd,n*2);rd+=n;if(rd==wr)rd=wr=0;return n;}
size_t psg_samples_available(void){return wr-rd;}
uint32_t psg_sample_rate(void){return rate;}
void psg_reset_leftover(void){}
