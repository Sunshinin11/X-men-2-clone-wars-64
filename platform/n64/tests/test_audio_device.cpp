#include <cassert>
#include <cstdio>
#include <vector>
#define N64 1
#ifndef X2_AUDIO_HEADER
#define X2_AUDIO_HEADER "../audio_n64.hpp"
#endif
#include X2_AUDIO_HEADER
static callback_t callback;
static bool dma_started=false;
volatile uint32_t g_x2_audio_ticks=0;
static unsigned pumps=0;
void x2_sound_pump(){pumps++;}
static unsigned starts=0,delivered=0,initializations=0,synthesis_frames=0,native_commands=0;
void audio_init(int hz,float){initializations++;assert(hz==12000);dma_started=false;}
int audio_get_frequency(){return 12000;}
int audio_get_buffer_length(){return 257;}
void audio_set_buffer_callback(callback_t c){callback=c;}
void audio_pause(bool){}
void audio_write_silence(){dma_started=true;starts++;}
void x2_sound_init(unsigned r){assert(r==12000);}
void x2_sound_command(uint8_t c,const uint8_t*a,unsigned n){assert(c==0x10&&n==1&&a[0]==0x27);native_commands++;}
X2SoundStats x2_sound_stats(){return {};}
unsigned x2_sound_frame(int16_t*out,unsigned cap){synthesis_frames++;assert(cap>=200);for(int i=0;i<400;i++)out[i]=(i&1)?-1234:1234;return 200;}
static void consume(unsigned n){assert(dma_started&&callback);std::vector<short>b(n*2+2,30000);callback(b.data()+1,n);assert(b.front()==30000&&b.back()==30000);for(unsigned i=1;i<=n*2;i++)if(b[i])delivered++;}
int main(){
 X2AudioSystem disabled;disabled.init(0);disabled.service();assert(initializations==0);
 X2AudioSystem native;native.init(3);assert(initializations==1&&starts==0&&callback==nullptr);
 for(unsigned i=0;i<1000;i++)native.service();
 assert(pumps==1000&&synthesis_frames==0);
 uint8_t z[8192]={};z[0x1b40]=0xff;z[0x1b41]=0x10;z[0x1b42]=0x27;
 native.process_ring(z,3);assert(native_commands==1);
 assert(native.telemetry.enabled&&native.telemetry.chip_instances==0);
 puts("AUDIO DEVICE PASS: native mode services RSP pump; no legacy callback or CPU synthesis; commands delivered");
}
