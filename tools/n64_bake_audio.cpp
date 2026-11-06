// Host-only asset preparation. The original sound machine never enters the N64 link.
#include "../platform/n64/sound_core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <cassert>
extern "C" { uint8_t g_rom[0x200000];uint8_t xm_ram_peek8(uint32_t){return 0;} }
static unsigned write_index;
static void send(unsigned c,const std::vector<uint8_t>& args){
 x2_sound_busreq(true);
 auto put=[](uint8_t v){x2_sound_write(0x1b40+write_index,v);write_index=(write_index+1)&63;};
 put(0xff);put(c);for(auto b:args)put(b);x2_sound_write(0x36,write_index);x2_sound_busreq(false);
}
static void advance(){int16_t pcm[1024];x2_sound_frame(pcm,512);}
static void init(){
 x2_sound_init(12000);x2_sound_reset(false);x2_sound_busreq(true);
 for(unsigned i=0;i<0x1866;i++)x2_sound_write(i,g_rom[0x1fe66c+i]);
 x2_sound_busreq(false);x2_sound_reset(true);write_index=0;
 for(int i=0;i<4;i++)advance();
 std::vector<uint8_t> args;for(unsigned a:{0x1618bcu,0x163bf2u,0x1641beu,0x17b8aau})for(unsigned b=0;b<3;b++)args.push_back(a>>(8*b));
 send(0x0b,args);for(int i=0;i<4;i++)advance();
}
static void be32(FILE*f,uint32_t v){for(int b=3;b>=0;b--)fputc(v>>(8*b),f);}
int main(){
 FILE*f=fopen("game/xmen2.bin","rb");assert(f);assert(fread(g_rom,1,sizeof g_rom,f)==sizeof g_rom);fclose(f);
 FILE*out=fopen("build/n64/native_audio/voices.pcm","wb");assert(out);
 struct Entry{uint32_t offset,count,rate,type;};std::vector<Entry> entries;
 unsigned offset=0,nonzero=0;
 for(unsigned patch=0;patch<352;patch++){
  unsigned p=patch<256?patch:0;
  if(patch>=256){for(p=0;p<256;p++){unsigned off=g_rom[0x1618bc+p*2]|g_rom[0x1618bd+p*2]<<8;if(g_rom[0x1618bc+off]==1)break;}assert(p<256);}
  unsigned off=g_rom[0x1618bc+p*2]|g_rom[0x1618bd+p*2]<<8;
  unsigned type=g_rom[0x1618bc+off],note=patch<256?48:patch-256;
  // DAC patches have a distinct asset for every sample note.
  if(patch<256&&type==1){entries.push_back({offset,0,12000,type});continue;}
  init();send(2,{0,(uint8_t)p});advance();send(0,{0,(uint8_t)note});
  unsigned pan=type==0?(g_rom[0x1618bc+off+4]>>6):3;
  Entry e{offset,0,12000,type|(pan<<9)};std::vector<int16_t> voice;
  for(unsigned frame=0;frame<180;frame++){
   int16_t pcm[1024];unsigned n=x2_sound_frame(pcm,512);
   for(unsigned i=0;i<n;i++){
    int v=(int(pcm[i*2])+pcm[i*2+1])/2;
    if(v)nonzero++;
    voice.push_back(v);
   }
  }
  // Find a stable sustain loop offline, keeping all waveform work off N64.
  uint64_t tail_energy=0;for(unsigned i=voice.size()-128;i<voice.size();i++)tail_energy+=std::abs(int(voice[i]));
  bool loop=type!=1&&tail_energy>1024;
  if(type==1){unsigned sid=note<48?note+48:note-48;loop=(g_rom[0x17b8aa+sid*12]&16)!=0;}
  if(loop){
   uint64_t best=~uint64_t(0);unsigned best_length=1024;
   for(unsigned span=128;span<=4096;span++){
    uint64_t cost=0;for(unsigned j=0;j<32;j++){int delta=int(voice[voice.size()-32+j])-voice[voice.size()-span-32+j];cost+=int64_t(delta)*delta;}
    if(cost<best){best=cost;best_length=span;}
   }
   e.type|=0x100|(best_length<<16);
  }else{while(!voice.empty()&&!voice.back())voice.pop_back();}
  for(int16_t v:voice){fputc(uint16_t(v)>>8,out);fputc(uint16_t(v)&255,out);}
  e.count=voice.size();offset+=e.count*2;entries.push_back(e);
  if(!(patch%32))std::fprintf(stderr,"baked voice %u/352\n",patch);
 }
 fclose(out);out=fopen("build/n64/native_audio/voices.idx","wb");assert(out);
 be32(out,0x58325632);be32(out,entries.size());for(auto&e:entries){be32(out,e.offset);be32(out,e.count);be32(out,e.rate);be32(out,e.type);}fclose(out);
 assert(nonzero);printf("Baked %zu native voice assets, %u bytes, %u nonzero samples\n",entries.size(),offset,nonzero);
}
