#include "../audio_commands.hpp"
#include <cassert>
#include <cstdio>
int main(){
 X2SoundCommands c;uint8_t z[8192]={};z[0x1b40]=0xff;z[0x1b41]=0x10;c.process(z,2);assert(c.plays==0);
 z[0x1b42]=0x27;c.process(z,3);assert(c.plays==1&&c.last_id==0x27);
 c.read=62;z[0x1b7e]=0xff;z[0x1b7f]=0x12;c.process(z,0);z[0x1b40]=0x27;c.process(z,1);assert(c.stops==1);
 // The init payload can contain FF; it must never be interpreted as a new command.
 X2SoundCommands init; unsigned calls=0;
 init.context=&calls;
 init.sink=[](void* p,uint8_t cmd,const uint8_t* a,unsigned n){
  ++*static_cast<unsigned*>(p);assert(cmd==0x0b&&n==12);
  for(unsigned i=0;i<n;i++)assert(a[i]==0xff);
 };
 init.byte(0xff);init.byte(0x0b);
 for(int i=0;i<11;i++){init.byte(0xff);assert(calls==0);}
 init.byte(0xff);assert(calls==1&&init.state==0&&init.unknown==0);
 X2SoundCommands params;params.byte(0xff);params.byte(0x1b);params.byte(0x0b);
 assert(params.state==2);params.byte(0xff);assert(params.state==0&&params.unknown==0);
 X2OutputTestTone tone;short buffer[2*257];for(int i=0;i<100;i++){tone.fill(buffer,257,12000);for(int j=0;j<257;j++)assert(buffer[2*j]==buffer[2*j+1]);}
 assert(tone.nonzero==24000&&tone.peak==4096&&tone.emitted==25700);
 for(short s:buffer)assert(s==0);
 puts("AUDIO PASS fragmented command, ring wrap, 24000 nonzero diagnostic samples, silence after 2s; no claim of physical playback");
}

