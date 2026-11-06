#include "../native_sequence.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>

int main(){
 std::vector<uint8_t> bank(1024);
 bank[0]=0;bank[1]=2;bank[512]=1;bank[513]=0x10;bank[514]=2;
 // Store a variable, branch over a note, then play a delayed note and end.
 const uint8_t program[]={0x70,11,3,0x71,11,0,4,1,12,0xc2,0x85,20,0x60};
 for(unsigned i=0;i<sizeof program;i++)bank[528+i]=program[i];
 unsigned notes=0;
 X2NativeSequence seq;
 seq.init(bank.data(),bank.size(),[](void* p,const X2NativeSequence::Event&e){
  if(e.kind==X2NativeSequence::Note){assert(e.value==20&&e.duration==5);++*static_cast<unsigned*>(p);}
 },&notes);
 seq.play(0);for(int i=0;i<20;i++)seq.tick();
 assert(notes==1&&seq.variables[11]==3&&seq.errors==0&&!seq.tracks[0].active);
 unsigned lifecycle=0;
 seq.init(bank.data(),bank.size(),[](void*p,const X2NativeSequence::Event&e){
  if(e.kind==X2NativeSequence::Finish)++*static_cast<unsigned*>(p);
  if(e.kind==X2NativeSequence::Clip)*static_cast<unsigned*>(p)+=10;
 },&lifecycle);
 seq.play(0);for(int i=0;i<20;i++)seq.tick();assert(lifecycle==1);
 seq.stop(255);assert(lifecycle==11);
 // Zero-delay goto to itself is bounded and reported, rather than hanging.
 bank[528]=0x6f;bank[529]=0xfd;bank[530]=0xff;
 seq.init(bank.data(),bank.size());seq.play(0);seq.tick();seq.tick();assert(seq.errors==1);
 // Real cartridge bank: all songs and player parameter choices must decode.
 std::ifstream f("build/n64/native_audio/sequence.bin",std::ios::binary);
 assert(f.good());bank.assign(std::istreambuf_iterator<char>(f),{});
 unsigned failures=0,total_notes=0;
 for(unsigned song=0;song<256;song++)for(unsigned character=0;character<8;character++){
  seq.init(bank.data(),bank.size());seq.variables[10]=1;seq.variables[11]=character;seq.variables[12]=character;
  seq.play(song);for(unsigned tick=0;tick<3600;tick++)seq.tick();
  total_notes+=seq.notes;
  if(seq.errors){std::printf("song=%02x character=%u errors=%u\n",song,character,seq.errors);failures++;}
 }
 std::printf("NATIVE SEQUENCE songs=256 parameter_sets=8 ticks=3600 notes=%u failures=%u\n",total_notes,failures);
 return failures?1:0;
}
