#pragma once
#include <stdint.h>
#include <stddef.h>
// Observe the original byte stream; published writes can split a command.
struct X2SoundCommands {
 using Sink = void (*)(void*, uint8_t, const uint8_t*, unsigned);
 uint8_t read=0,state=0,command=0,last_id=0;
 uint8_t args[12]={},used=0,needed=0;
 uint32_t bytes=0,plays=0,stops=0,stop_all=0,unknown=0;
 Sink sink=nullptr; void* context=nullptr;
 static unsigned argument_count(uint8_t c){
  switch(c){case 0x0b:return 12;case 0x10:case 0x12:case 0x20:return 1;case 0x1b:return 2;default:return 0;}
 }
 void dispatch(){
  switch(command){
   case 0x10:last_id=args[0];plays++;break;
   case 0x12:last_id=args[0];stops++;break;
   case 0x16:stop_all++;break;
   case 0x0b:case 0x0c:case 0x0d:case 0x1b:case 0x20:break;
   default:unknown++;break;
  }
  if(sink)sink(context,command,args,needed);
  state=0;
 }
 void byte(uint8_t b){
  if(state==2){args[used++]=b;if(used==needed)dispatch();}
  else if(state==1){command=b;used=0;needed=argument_count(b);if(needed)state=2;else dispatch();}
  else if(b==0xff)state=1;
 }
 void process(const uint8_t*z,uint8_t write){
  write&=63;
  while(read!=write){uint8_t b=z[0x1b40+read];read=(read+1)&63;bytes++;byte(b);}
 }
};
// Diagnostic only: never used as a substitute for the game's music/effects.
struct X2OutputTestTone {
 uint32_t phase=0,emitted=0,nonzero=0;int peak=0;
 void fill(short*out,size_t n,unsigned rate){
  for(size_t i=0;i<n;i++){
   short sample=emitted<rate*2 ? ((phase&0x80000000u)?4096:-4096):0;
   phase+=(uint32_t)((440ULL<<32)/rate);emitted++;
   if(sample){nonzero++;peak=4096;}
   out[i*2]=out[i*2+1]=sample;
  }
 }
};

