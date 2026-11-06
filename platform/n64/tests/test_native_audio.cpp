#include "../sound_core.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
extern "C" {unsigned char g_rom[0x200000];}
void x2_sound_command(uint8_t,const uint8_t*,unsigned);
static uint64_t render(unsigned song,unsigned character){
 x2_sound_init(12000);uint8_t p[]={10,1};x2_sound_command(0x1b,p,2);
 p[0]=11;p[1]=character;x2_sound_command(0x1b,p,2);
 assert(x2_sound_read(0x1b22+11)==character);
 uint8_t id=song;x2_sound_command(0x10,&id,1);
 uint64_t hash=1469598103934665603ull;unsigned nonzero=0;
 for(unsigned frame=0;frame<900;frame++){
  int16_t pcm[1024];unsigned n=x2_sound_frame(pcm,512);assert(n==200);
  auto stats=x2_sound_stats();assert(stats.active_fm+stats.active_psg<=8);
  for(unsigned i=0;i<n*2;i++){hash^=uint16_t(pcm[i]);hash*=1099511628211ull;if(pcm[i])nonzero++;}
 }
 assert(nonzero>1000);assert(x2_sound_stats().keyons>0);
 x2_sound_command(0x16,nullptr,0);int16_t pcm[1024];unsigned n=x2_sound_frame(pcm,512);
 for(unsigned i=0;i<n*2;i++)assert(pcm[i]==0);
 return hash;
}
int main(){
 FILE*f=fopen("game/xmen2.bin","rb");assert(f);assert(fread(g_rom,1,sizeof g_rom,f)==sizeof g_rom);fclose(f);
 // A one-shot must outlive a sequence that ends immediately after its note.
 // The global stop command must still clip that voice after its track ended.
 static unsigned char original[sizeof g_rom];memcpy(original,g_rom,sizeof g_rom);
 unsigned patch=0;for(;patch<256;patch++){unsigned o=g_rom[0x1618bc+patch*2]|g_rom[0x1618bd+patch*2]<<8;if(g_rom[0x1618bc+o]==1)break;}assert(patch<256);
 f=fopen("build/n64/native_audio/voices.idx","rb");assert(f);unsigned note=0;
 for(;note<96;note++){assert(!fseek(f,8+(256+note)*16+4,SEEK_SET));unsigned count=0;for(int b=0;b<4;b++){int v=fgetc(f);assert(v>=0);count=(count<<8)|unsigned(v);}if(count)break;}fclose(f);assert(note<96);
 auto*bank=g_rom+0x1641be;bank[0]=0;bank[1]=2;bank[512]=1;bank[513]=0x10;bank[514]=2;bank[515]=0;
 bank[528]=0x61;bank[529]=patch;bank[530]=note;bank[531]=0x60;
 x2_sound_init(12000);uint8_t song=0;x2_sound_command(0x10,&song,1);unsigned audible=0;
 for(int frame=0;frame<60;frame++){int16_t pcm[1024];unsigned n=x2_sound_frame(pcm,512);for(unsigned i=0;i<n*2;i++)if(pcm[i])audible++;}assert(audible);
 x2_sound_command(0x16,nullptr,0);int16_t silence[1024];unsigned n=x2_sound_frame(silence,512);for(unsigned i=0;i<n*2;i++)assert(!silence[i]);
 memcpy(g_rom,original,sizeof g_rom);
 auto first=render(0x27,1),again=render(0x27,1),second=render(0x27,2);
 assert(first==again&&first!=second);
 render(0x48,3);render(0x64,4);
 printf("NATIVE AUDIO PASS nonzero playback, deterministic samples, distinct character parts, nested songs, stop-all silence\n");
}
