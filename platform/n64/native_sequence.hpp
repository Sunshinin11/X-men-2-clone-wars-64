#pragma once
#include <stdint.h>
#include <stddef.h>

// GEMS data sequencer. This consumes musical commands, never CPU instructions.
// The output sink owns native sample voices; no chip registers are generated.
class X2NativeSequence {
public:
 enum Kind { Note, Patch, Envelope, Sustain, Bend, SampleRate, Attenuation, End, MusicTick, EffectTick, Begin, Finish, Clip };
 struct Event { Kind kind; unsigned slot; int value; uint16_t duration; uint8_t patch,priority,song; };
 using Sink=void(*)(void*,const Event&);
 struct Track {
  uint32_t pc=0,loop_pc[4]={};
  uint16_t delay=0,duration=0,wait=0;
  uint8_t song=0,number=0,patch=0,priority=0,loop_count[4]={},depth=0;
  bool active=false,paused=false,muted=false,sfx=false;
 } tracks[16];
 uint8_t variables[256]={};
 uint32_t errors=0,dropped_tracks=0,notes=0;
private:
 const uint8_t* data=nullptr;size_t length=0;
 Sink sink=nullptr;void* context=nullptr;
 unsigned beat_phase=0,beat_step=204;
 uint8_t fetch(Track&t){if(t.pc>=length){errors++;t.active=false;return 0x60;}return data[t.pc++];}
 uint32_t word(unsigned p,unsigned n){if(p>length||n>length-p){errors++;return 0;}uint32_t v=0;for(unsigned i=0;i<n;i++)v|=uint32_t(data[p+i])<<(8*i);return v;}
 void emit(Kind k,unsigned i,int value){auto&t=tracks[i];if(sink)sink(context,{k,i,value,t.duration,t.patch,t.priority,t.song});}
 void end(unsigned i){if(tracks[i].active){emit(End,i,0);tracks[i].active=false;}}
 void run(unsigned i){
  auto&t=tracks[i];
  if(t.wait){--t.wait;if(t.wait)return;}
  // A malformed zero-delay cycle must never hang the game thread.
  for(unsigned budget=0;budget<4096&&t.active&&!t.paused;budget++){
   unsigned b=fetch(t);if(!t.active)return;
   if(b&128){
    unsigned type=b&192;uint16_t value=b&63;
    while(t.pc<length&&(data[t.pc]&192)==type)value=uint16_t((value<<6)|(fetch(t)&63));
    if(type==192)t.delay=value;else t.duration=value;
    continue;
   }
   bool delayed=true;
   if(b<96){if(!t.muted){notes++;emit(Note,i,b);}}
   else switch(b){
    case 0x60:emit(Finish,i,0);t.active=false;return;
    case 0x61:t.patch=fetch(t);emit(Patch,i,t.patch);break;
    case 0x62:emit(Envelope,i,fetch(t));break;
    case 0x63:break;
    case 0x64:{unsigned count=fetch(t);if(t.depth==4||!count){errors++;end(i);return;}t.loop_count[t.depth]=count;t.loop_pc[t.depth++]=t.pc;delayed=false;break;}
    case 0x65:{if(!t.depth){errors++;end(i);return;}unsigned d=t.depth-1;if(t.loop_count[d]==127||--t.loop_count[d])t.pc=t.loop_pc[d];else t.depth--;delayed=false;break;}
    case 0x66:emit(Envelope,i,256+fetch(t));break;
    case 0x67:emit(Sustain,i,fetch(t));break;
    case 0x68:beat_step=((fetch(t)+40)*218)>>7;break;
    case 0x69:{unsigned m=fetch(t);for(auto&other:tracks)if(other.active&&other.song==t.song&&other.number==(m&15))other.muted=!(m&16);break;}
    case 0x6a:t.priority=fetch(t);break;
    case 0x6b:{unsigned song=fetch(t);play(song);break;}
    case 0x6c:{unsigned lo=fetch(t),hi=fetch(t);emit(Bend,i,int16_t(lo|(hi<<8)));break;}
    case 0x6d:t.sfx=true;break;
    case 0x6e:emit(SampleRate,i,fetch(t));break;
    case 0x6f:{unsigned lo=fetch(t),hi=fetch(t);int64_t target=int64_t(t.pc)+int16_t(lo|(hi<<8));if(target<0||uint64_t(target)>=length){errors++;end(i);return;}t.pc=uint32_t(target);delayed=false;break;}
    case 0x70:{unsigned v=fetch(t);variables[v]=fetch(t);break;}
    case 0x71:{unsigned v=variables[fetch(t)],rel=fetch(t),x=fetch(t),skip=fetch(t);bool ok=false;switch(rel){case 0:ok=v==x;break;case 1:ok=v!=x;break;case 2:ok=v>x;break;case 3:ok=v>=x;break;case 4:ok=v<x;break;case 5:ok=v<=x;break;default:errors++;end(i);return;}if(!ok)t.pc+=skip;delayed=false;break;}
    case 0x72:{unsigned code=fetch(t),value=fetch(t);switch(code){case 0:stop(value);break;case 1:pause(value);break;case 2:resume();break;case 3:pause(variables[0]);break;case 4:emit(Attenuation,i,256+value);break;case 5:emit(Attenuation,i,value);break;default:errors++;end(i);return;}break;}
    default:errors++;end(i);return;
   }
   if(delayed&&t.delay){t.wait=t.delay;return;}
  }
  if(t.active&&!t.paused){errors++;end(i);}
 }
public:
 void init(const uint8_t* bank,size_t size,Sink output=nullptr,void* user=nullptr){
  *this=X2NativeSequence{};data=bank;length=size;sink=output;context=user;
 }
 void play(unsigned song){
  if(song>=256||length<512){errors++;return;}
  unsigned header=word(song*2,2);if(header<512||header>=length){errors++;return;}
  unsigned count=data[header];if(header+1+count*3>length){errors++;return;}
  unsigned number=0;
  for(unsigned i=0;i<16&&number<count;i++)if(!tracks[i].active){
   uint32_t pc=word(header+1+number*3,3);
   if(pc<512||pc>=length){errors++;return;}
   tracks[i]=Track{};tracks[i].pc=pc;tracks[i].song=song;tracks[i].number=number++;tracks[i].active=true;emit(Begin,i,0);
  }
  dropped_tracks+=count-number;
 }
 void stop(unsigned song){for(unsigned i=0;i<16;i++)if(song==255||tracks[i].song==song)end(i);emit(Clip,0,0);}
 void stop_all(){for(unsigned i=0;i<16;i++)end(i);emit(Clip,0,0);}
 void pause(unsigned song){for(unsigned i=0;i<16;i++)if(tracks[i].active&&tracks[i].song==song){tracks[i].paused=true;emit(End,i,0);}emit(Clip,0,0);}
 void pause_all(){for(unsigned i=0;i<16;i++)if(tracks[i].active){tracks[i].paused=true;emit(End,i,0);}emit(Clip,0,0);}
 void resume(){for(auto&t:tracks)t.paused=false;}
 void tick(){
  // A 60 Hz effects tick and a fractional 24-subbeat music clock.
  emit(EffectTick,0,0);
  for(unsigned i=0;i<16;i++)if(tracks[i].active&&!tracks[i].paused&&tracks[i].sfx)run(i);
  beat_phase+=beat_step;
  while(beat_phase>=256){beat_phase-=256;emit(MusicTick,0,0);for(unsigned i=0;i<16;i++)if(tracks[i].active&&!tracks[i].paused&&!tracks[i].sfx)run(i);}
 }
};
