// Native sample playback. Instrument preparation runs only on the host.
#ifndef X2_AUDIO_OPT
#define X2_AUDIO_OPT 1
#endif
#include "sound_core.hpp"
#include "native_sequence.hpp"
#include <cstdio>
#include <cstring>
#include "pitch_table.hpp"
#ifdef N64
#include <libdragon.h>
#endif
#include <cstdlib>
extern "C" unsigned char g_rom[0x200000];
namespace {
constexpr unsigned CHUNK=1024;
constexpr unsigned VOICE_COUNT=8;
struct Asset{uint32_t offset,count,rate,type;} assets[352];
struct Voice{
 uint64_t position=0;uint32_t step=65536,cache_start=~0u;
 uint16_t remaining=0,asset=0;uint8_t note=48,slot=0,priority=0,fade=0;
 bool active=false,sfx=false,allow_loop=true;alignas(16) int16_t cache[CHUNK];
} voices[VOICE_COUNT];
static_assert(offsetof(Voice,cache)%16==0&&sizeof(Voice)%16==0,"DMA cache alignment");
struct Mod{
 int16_t bend=0,env_value=0,env_delta=0;unsigned env_pos=0;
 uint8_t env=0,env_count=0,atten=0,rate=4;
 bool retrigger=false,env_active=false,sustain=false;
} mods[16];
X2NativeSequence seq;X2SoundStats stats;
uint8_t mailbox[8192];FILE* samples=nullptr;
unsigned output_rate=12000,sample_phase=0,master_atten=0;
#ifdef N64
pi_addr_t sample_rom=0;
// N64: each voice is one libdragon mixer channel. The RSP resamples, applies
// volume and pan and mixes; the CPU only runs the sequencer and streams the
// PCM from the cartridge when the mixer asks for it.
waveform_t waves[VOICE_COUNT];
unsigned tick_left=0;
static unsigned index_of(const Voice&v){return unsigned(&v-voices);}
// Rate variants of every voice (1, 1/2, 1/4, 1/8; rate = exact ratio in 1/65536).
// A note plays the variant that keeps it at most 2 source samples per output
// sample, so a high note no longer streams 4-16x the data of its base pitch.
Asset variants[352][4];
uint8_t variant_of[VOICE_COUNT];
static unsigned pick_variant(unsigned id,uint32_t step){
 unsigned k=0;while(k<3&&variants[id][k+1].count&&(uint64_t(step)*variants[id][k].rate>>16)>131072)k++;
 return k;
}
static void voice_read(void*ctx,samplebuffer_t*sbuf,int wpos,int wlen,bool){
 // One asynchronous PI transfer per contiguous run of the ring. A pending
 // transfer must complete before its ticket can be reused.
 const Asset&a=*static_cast<const Asset*>(ctx);
 while(wlen>0){
  int slot=(sbuf->head+sbuf->widx)%sbuf->size,room=sbuf->size-slot;
  int n=wlen<=room?wlen:room>=8?room:(wlen<sbuf->margin_units?wlen:sbuf->margin_units);
  void*ram=samplebuffer_append(sbuf,n);
  samplebuffer_dma_wait(sbuf);
  sbuf->dma_ticket=dma_read_async(ram,sample_rom+a.offset+unsigned(wpos)*2,unsigned(n)*2);
  wlen-=n;wpos+=n;
 }
}
static void stop(Voice&v){v.active=false;mixer_ch_stop(index_of(v));}
static void apply_volume(Voice&v);
static void apply_freq(Voice&v){
 const Asset&a=variants[v.asset][variant_of[index_of(v)]];
 float f=float(v.step)*float(a.rate)/65536.0f*float(output_rate)/65536.0f;
 float top=float(output_rate)*4;
 if(f>top)f=top;
 mixer_ch_set_freq(index_of(v),f);
}
#else
static void stop(Voice&v){v.active=false;}
static void apply_volume(Voice&){}
static void apply_freq(Voice&){}
#endif
static uint32_t read32(FILE*f){uint32_t v=0;for(int i=0;i<4;i++){int b=fgetc(f);if(b<0)std::abort();v=(v<<8)|unsigned(b);}return v;}
static unsigned env_byte(unsigned p){return p<0x5cc?g_rom[0x163bf2+p]:0;}
static void envelope(unsigned slot){
 auto&m=mods[slot];unsigned p=unsigned(m.env)*2;
 if(p+1>=0x5cc){m.env_active=false;return;}
 p=env_byte(p)|(env_byte(p+1)<<8);
 if(p+2>=0x5cc){m.env_active=false;return;}
 m.env_value=int16_t(env_byte(p)|(env_byte(p+1)<<8));m.env_pos=p+2;m.env_count=0;m.env_active=true;
}
static void tune(Voice&v){
 auto&a=assets[v.asset];auto&m=mods[v.slot];
 unsigned pi=(v.asset<256?v.asset:seq.tracks[v.slot].patch)*2;
 unsigned po=g_rom[0x1618bc+pi]|g_rom[0x1618bd+pi]<<8;
 unsigned type=a.type&255;
 bool fixed_noise=type==3&&((g_rom[0x1618bc+po+1]&3)!=3);
 bool fixed=type==1||fixed_noise||(type==0&&(g_rom[0x1618bc+po+2]&64));
 int pitch=fixed?0:(int(v.note)-48)*256+int(m.bend)+int(m.env_value);
 if(pitch< -12288)pitch=-12288;
 if(pitch>12288)pitch=12288;
 int octave=pitch/3072,fraction=pitch%3072;
 if(fraction<0){fraction+=3072;--octave;}
 uint64_t ratio=x2_pitch_ratio[fraction];
 if(octave>=0)ratio<<=octave;else ratio>>=-octave;
 v.step=uint32_t((ratio*12000/output_rate)>>8);
 if(type==1){
  unsigned note=v.note,sample_id=note<48?note+48:note-48;
  unsigned flags=g_rom[0x17b8aa+sample_id*12];
  unsigned first_dac=0;for(;first_dac<256;first_dac++)if(assets[first_dac].type==1)break;
  unsigned off=g_rom[0x1618bc+first_dac*2]|g_rom[0x1618bd+first_dac*2]<<8;
  unsigned baked=g_rom[0x1618bc+off+1];if(baked==4)baked=flags&15;
  unsigned current=m.rate==4?(flags&15):m.rate;
  v.step=uint32_t((65536ull*12000*(baked?baked:1024))/(uint64_t(output_rate)*(current?current:1024)));
 }
 if(!v.step)v.step=1;
#ifdef X2_STEP_PROBE
 {unsigned b=v.step>>15;g_x2_step_hist[b>32?32:b]++;}
#endif
 if(v.active)apply_freq(v);
}
static void clock(bool sfx){
 for(unsigned i=0;i<16;i++){
  auto&m=mods[i];if(seq.tracks[i].sfx!=sfx||seq.tracks[i].paused)continue;
  if(m.env_active){
   if(!m.env_count){m.env_count=env_byte(m.env_pos++);if(!m.env_count){m.env_active=false;continue;}
    m.env_delta=int16_t(env_byte(m.env_pos)|(env_byte(m.env_pos+1)<<8));m.env_pos+=2;}
   m.env_count--;m.env_value=int16_t(uint16_t(m.env_value)+uint16_t(m.env_delta));
   for(auto&v:voices)if(v.active&&v.slot==i)tune(v);
  }
 }
 for(auto&v:voices)if(v.active&&v.sfx==sfx&&v.remaining&&!mods[v.slot].sustain){if(!--v.remaining){
  auto&a=assets[v.asset];
  if((a.type&255)==1){unsigned sid=v.note<48?v.note+48:v.note-48;unsigned flags=g_rom[0x17b8aa+sid*12];
   if(flags&32)v.fade=64;
   else if(flags&16){v.allow_loop=false;unsigned loop=a.type>>16;
#ifdef N64
    unsigned i=index_of(v);mixer_ch_set_loop(i,false);
    const Asset&va=variants[v.asset][variant_of[i]];loop=va.type>>16;
    if(loop&&loop<va.count&&mixer_ch_get_pos(i)<double(va.count-loop))mixer_ch_set_pos(i,double(va.count-loop));
#else
    if(loop&&loop<a.count&&(v.position>>16)<a.count-loop)v.position=uint64_t(a.count-loop)<<16;
#endif
   }
  }else v.fade=64;
#ifdef N64
  if(v.fade)stop(v); // (64-sample release: about 5 ms)
#endif
 }}
}
static void event(void*,const X2NativeSequence::Event&e){
 auto&m=mods[e.slot];
 switch(e.kind){
  case X2NativeSequence::Begin:m=Mod{};return;
  case X2NativeSequence::Finish:return;
  case X2NativeSequence::Clip:for(auto&v:voices)if(v.active&&(!seq.tracks[v.slot].active||seq.tracks[v.slot].paused))stop(v);return;
  case X2NativeSequence::MusicTick:clock(false);return;
  case X2NativeSequence::EffectTick:clock(true);return;
  case X2NativeSequence::End:for(auto&v:voices)if(v.active&&v.slot==e.slot)stop(v);return;
  case X2NativeSequence::Envelope:if(e.value>=256)m.retrigger=(e.value&255)!=0;else{m.env=e.value;if(!m.retrigger)envelope(e.slot);}return;
  case X2NativeSequence::Bend:m.bend=e.value;for(auto&v:voices)if(v.active&&v.slot==e.slot)tune(v);return;
  case X2NativeSequence::Attenuation:if(e.value>=256)master_atten=e.value&127;else m.atten=e.value&127;return;
  case X2NativeSequence::Sustain:m.sustain=e.value!=0;return;
  case X2NativeSequence::Patch:{unsigned off=g_rom[0x1618bc+e.patch*2]|g_rom[0x1618bd+e.patch*2]<<8;m.rate=g_rom[0x1618bc+off+1];return;}
  case X2NativeSequence::SampleRate:m.rate=e.value;for(auto&v:voices)if(v.active&&v.slot==e.slot)tune(v);return;
  case X2NativeSequence::Note:break;
 }
 unsigned id=e.patch;if((assets[id].type&255)==1)id=256+e.value;
 if(id>=352||!assets[id].count)return;
 Voice* chosen=nullptr;
 for(auto&v:voices)if(!v.active){chosen=&v;break;}
 if(!chosen){for(auto&v:voices)if(!chosen||v.priority<chosen->priority)chosen=&v;if(chosen->priority>e.priority)return;}
 // Invalidate cached samples without clearing 2 KiB of disposable PCM on every note.
 chosen->position=0;chosen->step=65536;chosen->cache_start=~0u;
 chosen->fade=0;chosen->active=false;chosen->allow_loop=true;
 chosen->asset=id;chosen->slot=e.slot;chosen->note=e.value;
 chosen->priority=e.priority;chosen->remaining=e.duration;chosen->sfx=seq.tracks[e.slot].sfx;
 if(m.retrigger)envelope(e.slot);
 tune(*chosen);chosen->active=true;stats.keyons++;
#ifdef N64
 {
  unsigned i=index_of(*chosen);variant_of[i]=uint8_t(pick_variant(id,chosen->step));
  auto&a=variants[id][variant_of[i]];unsigned loop=a.type>>16;
  waveform_t&w=waves[i];w=waveform_t{};
  w.name="x2";w.bits=16;w.channels=1;w.frequency=float(output_rate);w.len=int(a.count);
  w.loop_len=((a.type&0x100)&&loop&&loop<=a.count)?int(loop):0;
  w.read=voice_read;w.ctx=&a;w.async_read=true;
  mixer_ch_play(i,&w);apply_freq(*chosen);apply_volume(*chosen);
 }
#endif
}
static int sample(Voice&v){
 auto&a=assets[v.asset];unsigned pos=unsigned(v.position>>16);
 if(pos>=a.count){
  unsigned loop=a.type>>16;
  if(v.allow_loop&&(a.type&0x100)&&loop&&loop<=a.count){while((v.position>>16)>=a.count)v.position-=uint64_t(loop)<<16;pos=unsigned(v.position>>16);}
  else{v.active=false;return 0;}
 }
 if(v.cache_start==~0u||pos<v.cache_start||pos>=v.cache_start+CHUNK){
  v.cache_start=(pos/CHUNK)*CHUNK;unsigned count=a.count-v.cache_start;if(count>CHUNK)count=CHUNK;
#ifdef N64
  // These aligned cache lines contain only disposable sample data. Invalidate
  // them before PI writes RDRAM; PCM is already native big-endian on N64.
  data_cache_hit_invalidate(v.cache,sizeof v.cache);
  dma_read(v.cache,sample_rom+a.offset+v.cache_start*2,count*2);
#else
  unsigned char bytes[CHUNK*2];
  if(fseek(samples,long(a.offset+v.cache_start*2),SEEK_SET)||fread(bytes,2,count,samples)!=count){v.active=false;return 0;}
  for(unsigned i=0;i<count;i++)v.cache[i]=int16_t((unsigned(bytes[i*2])<<8)|bytes[i*2+1]);
#endif
 }
 int value=v.cache[pos-v.cache_start];v.position+=v.step;
#ifdef X2_STEP_PROBE
 {g_x2_bytes_now+=v.step/32768.0;uint32_t st=v.step;while(st>131072)st>>=1;g_x2_bytes_oct+=st/32768.0;}
#endif
 unsigned attenuation=master_atten+mods[v.slot].atten;if(attenuation>127)attenuation=127;
 value=value*int(127-attenuation)/127;
 if(v.fade){value=value*v.fade/64;if(!--v.fade)v.active=false;}
 return value;
}
}
void x2_sound_init(unsigned rate){
 output_rate=rate;sample_phase=0;master_atten=0;stats={};memset(mailbox,0,sizeof mailbox);
 for(auto&v:voices)v=Voice{};
 for(auto&m:mods)m=Mod{};
#ifdef N64
 const char* index_path="rom:/voices.idx";
#else
 const char* index_path="build/n64/native_audio/voices.idx";const char* sample_path="build/n64/native_audio/voices.pcm";
#endif
#ifdef N64
 FILE*f=fopen(index_path,"rb");if(!f||read32(f)!=0x58325633||read32(f)!=352)std::abort();
 for(unsigned id=0;id<352;id++)for(auto&a:variants[id]){a.offset=read32(f);a.count=read32(f);a.rate=read32(f);a.type=read32(f);}
 fclose(f);for(unsigned id=0;id<352;id++)assets[id]=variants[id][0];
#else
 FILE*f=fopen(index_path,"rb");if(!f||read32(f)!=0x58325632||read32(f)!=352)std::abort();
 for(auto&a:assets){a.offset=read32(f);a.count=read32(f);a.rate=read32(f);a.type=read32(f);}fclose(f);
#endif
 if(samples)fclose(samples);
#ifdef N64
 sample_rom=dfs_rom_addr("/voices.pcm");if(!sample_rom)std::abort();
 static bool mixing=false;
 if(!mixing){mixer_init(VOICE_COUNT);for(unsigned i=0;i<VOICE_COUNT;i++)mixer_ch_set_limits(i,16,float(output_rate)*4,0);mixing=true;}
 tick_left=0;
#else
 samples=fopen(sample_path,"rb");if(!samples)std::abort();
#endif
 seq.init(g_rom+0x1641be,0x17b8aa-0x1641be,event,nullptr);
}
void x2_sound_command(uint8_t c,const uint8_t*a,unsigned n){
 switch(c){
  case 0x0b:if(n==12){seq.stop_all();seq.init(g_rom+0x1641be,0x17b8aa-0x1641be,event,nullptr);for(auto&m:mods)m=Mod{};}break;
  case 0x10:if(n==1)seq.play(a[0]);break;
  case 0x12:if(n==1){if(a[0]==255)seq.stop_all();else seq.stop(a[0]);}break;
  case 0x16:seq.stop_all();break;
  case 0x0c:seq.pause_all();for(auto&v:voices)if(v.active)stop(v);break;
  case 0x0d:seq.resume();break;
  case 0x1b:if(n==2)seq.variables[a[0]]=a[1];break;
  case 0x20:if(n==1)master_atten=a[0]&127;break;
  default:break;
 }
}
uint8_t x2_sound_read(unsigned a){a&=8191;if(a>=0x1b22&&a<0x1b40)return seq.variables[a-0x1b22];return mailbox[a];}
void x2_sound_write(unsigned a,uint8_t v){a&=8191;mailbox[a]=v;if(a>=0x1b22&&a<0x1b40)seq.variables[a-0x1b22]=v;}
void x2_sound_busreq(bool){}
uint8_t x2_sound_busstatus(){return 0;}
void x2_sound_reset(bool){}
X2SoundStats x2_sound_stats(){stats.active_fm=0;stats.active_psg=0;for(auto&v:voices)if(v.active){if((assets[v.asset].type&255)>=2)stats.active_psg++;else stats.active_fm++;}return stats;}
unsigned x2_sound_frame(int16_t*out,unsigned cap){
 sample_phase+=output_rate;unsigned count=sample_phase/60;sample_phase%=60;if(count>cap)std::abort();
 if(!mailbox[0x1b20])seq.tick();
 for(unsigned i=0;i<count;i++){
  int channels[2]={};
  for(auto&v:voices)if(v.active){int value=sample(v);unsigned pan=(assets[v.asset].type>>9)&3;
   if(pan==3){channels[0]+=value;channels[1]+=value;}
   else if(pan&2)channels[0]+=value*2;
   else if(pan&1)channels[1]+=value*2;
  }
  for(unsigned ch=0;ch<2;ch++){
   int sum=channels[ch]/2;if(sum>32767)sum=32767;if(sum< -32768)sum=-32768;
   out[2*i+ch]=sum;
   if(sum){stats.nonzero++;unsigned peak=unsigned(sum<0?-sum:sum);if(peak>stats.peak)stats.peak=peak;}
  }
 }
 stats.frames++;stats.samples+=count;return count;
}

#ifdef N64
namespace {
static void apply_volume(Voice&v){
 unsigned attenuation=master_atten+mods[v.slot].atten;if(attenuation>127)attenuation=127;
 float g=float(127-attenuation)/127.0f;unsigned pan=(assets[v.asset].type>>9)&3;
 // Same weights as the CPU mix: both sides at half, or one side at full.
 float l=pan==3?g*0.5f:(pan&2)?g:0.0f,r=pan==3?g*0.5f:(pan&1)?g:0.0f;
 mixer_ch_set_vol(index_of(v),l,r);
}
}
// The sequencer is a mixer event: the mixer calls it every 1/60 s of output,
// at that exact sample, while it builds the RSP mix. Service is delegated to
// libdragon; RSP/PI work and any ticket waits are not assumed to be free.
static int sequencer_tick(void*){
 for(auto&v:voices)if(v.active&&!mixer_ch_playing(index_of(v)))v.active=false;
 if(!mailbox[0x1b20])seq.tick();
 for(auto&v:voices)if(v.active)apply_volume(v);
 sample_phase+=output_rate;int n=int(sample_phase/60);sample_phase%=60;
 stats.frames++;stats.samples+=unsigned(n);
 return n?n:1;
}
bool x2_sound_pump_sliced(int max_queued,int max_samples){
 static bool registered=false;
 static int num_bufs=0,buf_len=0;
 static short* active_buf=nullptr;
 static int active_pos=0;
 if(!registered){
  mixer_add_event(0,sequencer_tick,nullptr);
  num_bufs=audio_get_num_buffers();
  buf_len=audio_get_buffer_length();
  registered=true;
  max_queued=999;
  max_samples=0;
 }
 const int queued=audio_get_queued_buffers();
 const int limit=max_queued<0?(num_bufs>0?num_bufs-1:2):max_queued;
 bool did_try=false;
 if(!X2_AUDIO_OPT){
  mixer_try_play();
  did_try=true;
 }else if(queued<=limit){
  const bool fill_all=(max_queued>=999);
  do{
   if(!active_buf){
    if(!audio_can_write())break;
    active_buf=audio_write_begin();
    active_pos=0;
   }
   int rem=buf_len-active_pos;
   int chunk=(!fill_all&&max_samples>0&&max_samples<rem)?max_samples:rem;
   chunk&=~1;
   if(chunk<=0)break;
   mixer_poll(active_buf+active_pos*2,chunk);
   active_pos+=chunk;
   did_try=true;
   if(active_pos>=buf_len){
    audio_write_end();
    active_buf=nullptr;
    active_pos=0;
   }
  }while(fill_all&&audio_can_write());
 }
 return did_try;
}
bool x2_sound_pump_if_below(int max_queued){
 return x2_sound_pump_sliced(max_queued,0);
}
void x2_sound_pump(){
#if X2_AUDIO_OPT
 (void)x2_sound_pump_if_below(-1);
#else
 (void)x2_sound_pump_if_below(999);
#endif
}
#endif
