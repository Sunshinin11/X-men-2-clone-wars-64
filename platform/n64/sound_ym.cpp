// Reduced-rate OPN2 rendering, using original ymfm register/patch/algorithm engine.
// ymfm is BSD-3-Clause; see the vendored ymfm license. No ID-to-tone substitutes.
#include "../../segagenesisrecomp/runner/external/ymfm/src/ymfm_opn.cpp"
#include <stdint.h>
#include <string.h>
extern "C" {
#include "audio/ym2612.h"
}
namespace {
constexpr unsigned MASTER=53693175;
struct Interface:ymfm::ymfm_interface{} iface;
struct Chip:ymfm::ym2612 {
 Chip():ym2612(iface){}
 void step(){m_fm.clock(fm_engine::ALL_CHANNELS);}
 void sample(output_data&out){
  // Operator/envelope clocks retain original pitch. Expensive waveform evaluation
  // occurs ONLY at the requested output rate, not 53267 waveform samples/sec.
  m_fm.output(out.clear(),5,256,m_dac_enable?0x1f:0x3f);
  if(m_dac_enable){int32_t d=int16_t(m_dac_data<<7)>>7;
   if(m_fm.regs().ch_output_0(0x102))out.data[0]+=d;
   if(m_fm.regs().ch_output_1(0x102))out.data[1]+=d;
  }
  for(int c=0;c<2;c++)out.data[c]=out.data[c]*16;
 }
} chip;
unsigned rate=12000,clocks;uint64_t phase;
int16_t samples[8192*2];unsigned wr,rd;
}
extern "C" void x2_ym_rate(unsigned r){rate=r;}
extern "C" void ym2612_init(){chip.reset();clocks=0;phase=0;wr=rd=0;}
extern "C" void ym2612_set_model(int){}
extern "C" void ym2612_advance(uint32_t delta){
 // Split at output boundaries so clock counts and event timestamps carry exactly.
 while(delta){unsigned remain=(unsigned)((MASTER-phase+rate-1)/rate);unsigned n=delta<remain?delta:remain;
  delta-=n;clocks+=n;while(clocks>=1008){clocks-=1008;chip.step();}
  phase+=(uint64_t)n*rate;
  if(phase>=MASTER){phase-=MASTER;Chip::output_data out;chip.sample(out);
   if(wr<8192){for(int c=0;c<2;c++){int v=out.data[c];samples[wr*2+c]=(int16_t)(v>32767?32767:v< -32768?-32768:v);}wr++;}}
 }
}
extern "C" void ym2612_write(uint8_t p,uint8_t v){chip.write(p,v);}
extern "C" size_t ym2612_render(int16_t*out,size_t n){unsigned available=wr-rd;if(n>available)n=available;memcpy(out,samples+rd*2,n*4);rd+=n;if(rd==wr)rd=wr=0;return n;}
extern "C" size_t ym2612_samples_available(){return wr-rd;}
extern "C" uint32_t ym2612_sample_rate(){return rate;}
