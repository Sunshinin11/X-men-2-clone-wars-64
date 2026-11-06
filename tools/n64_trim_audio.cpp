// Host-only. Shortens the baked voices: every looped voice was rendered for a
// fixed 3 s and looped only at its very end, so a note streamed up to 3 s of
// PCM before repeating. Here the loop starts as soon as the remaining render is
// a repetition of that loop (within -26 dB of error), so what plays is the same
// sound while the data shrinks. Input: voices.pcm/idx; output: voices_trim.*.
//
// It then writes voices_v.*: each voice at 1, 1/2, 1/4 and 1/8 of its sample
// rate (band-limited). A note far above the voice's base pitch plays a smaller
// variant, so no voice reads more than about two source samples per output
// sample: the cartridge traffic of high notes drops by up to 8x, and those
// notes alias less. Loops keep an integer length in every variant; the exact
// rate ratio of each variant is stored so the pitch stays exact.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <cmath>
static uint32_t rd32(FILE*f){uint32_t v=0;for(int i=0;i<4;i++)v=(v<<8)|unsigned(fgetc(f));return v;}
static void be32(FILE*f,uint32_t v){for(int b=3;b>=0;b--)fputc(v>>(8*b),f);}
int main(int argc,char**argv){
 const char*dir=argc>1?argv[1]:"build/n64/native_audio";char p[512];
 snprintf(p,sizeof p,"%s/voices.idx",dir);FILE*fi=fopen(p,"rb");assert(fi);
 assert(rd32(fi)==0x58325632);unsigned n=rd32(fi);
 struct E{uint32_t offset,count,rate,type;};std::vector<E>in(n);
 for(auto&e:in){e.offset=rd32(fi);e.count=rd32(fi);e.rate=rd32(fi);e.type=rd32(fi);}fclose(fi);
 snprintf(p,sizeof p,"%s/voices.pcm",dir);FILE*fp=fopen(p,"rb");assert(fp);
 snprintf(p,sizeof p,"%s/voices_trim.pcm",dir);FILE*op=fopen(p,"wb");assert(op);
 std::vector<std::vector<int16_t>> trimmed;
 uint64_t before=0,after=0;unsigned shortened=0;uint32_t offset=0;std::vector<E>out;
 for(auto e:in){
  std::vector<int16_t>v(e.count);fseek(fp,long(e.offset),SEEK_SET);
  for(auto&s:v){int h=fgetc(fp),l=fgetc(fp);s=int16_t((h<<8)|l);}
  before+=v.size()*2;
  unsigned loop=e.type>>16;
  if((e.type&0x100)&&loop&&loop<v.size()){
   const unsigned N=unsigned(v.size()),S=loop;
   double sig=0;for(unsigned i=N-S;i<N;i++)sig+=double(v[i])*v[i];sig/=S; // loop power
   unsigned best=N;
   for(unsigned end=S+256;end+S<=N;end+=64){
    double err=0;bool ok=true;
    for(unsigned j=end;j<N;j++){double d=double(v[j])-v[end-S+((j-end)%S)];err+=d*d;
     if(!((j-end+1)%1024)){if(err>(j-end+1)*sig*0.0025){ok=false;break;}}}
    if(ok&&err<=(N-end)*sig*0.0025){best=end;break;}
   }
   if(best<N){v.resize(best);shortened++;}
  }
  trimmed.push_back(v);
  for(int16_t s:v){fputc(uint16_t(s)>>8,op);fputc(uint16_t(s)&255,op);}
  e.offset=offset;e.count=uint32_t(v.size());offset+=e.count*2;after+=v.size()*2;out.push_back(e);
 }
 fclose(op);fclose(fp);
 snprintf(p,sizeof p,"%s/voices_trim.idx",dir);FILE*oi=fopen(p,"wb");assert(oi);
 be32(oi,0x58325632);be32(oi,n);for(auto&e:out){be32(oi,e.offset);be32(oi,e.count);be32(oi,e.rate);be32(oi,e.type);}fclose(oi);
 // Variants. Output position m of variant k samples the source at
 // t = N - (M - m) / r, anchored to the end so that the loop maps exactly.
 snprintf(p,sizeof p,"%s/voices_v.pcm",dir);FILE*vp=fopen(p,"wb");assert(vp);
 struct V{uint32_t offset,count,ratio,type;};std::vector<V>var;uint32_t voff=0;uint64_t vbytes=0;
 for(unsigned a=0;a<n;a++){
  const auto&x=trimmed[a];const E&e=out[a];unsigned N=unsigned(x.size()),L=e.type>>16;bool looped=(e.type&0x100)&&L&&L<=N;
  for(unsigned k=0;k<4;k++){
   V o{voff,0,65536,e.type};
   if(!N){var.push_back(o);continue;}
   std::vector<int16_t>y;
   if(!k)y=x;
   else{
    double r=1.0/(1u<<k);unsigned Lk=0;
    if(looped){Lk=unsigned(std::lround(L*r));if(Lk<16)Lk=16;r=double(Lk)/L;}
    unsigned M=unsigned(std::ceil(N*r));
    double fc=0.45*r;int half=int(std::ceil(12/r));  // cutoff in source cycles/sample
    auto src=[&](long i)->double{
     if(i<0)return 0;
     if(i>=long(N)){if(!looped)return 0;i=long(N)-long(L)+(i-long(N))%long(L);}
     return x[size_t(i)];};
    y.resize(M);
    for(unsigned m=0;m<M;m++){
     double t=double(N)-double(M-m)/r;long c=long(std::floor(t));double acc=0,wsum=0;
     for(long i=c-half+1;i<=c+half;i++){
      double d=t-double(i);double w=0.5+0.5*std::cos(M_PI*d/half);if(std::fabs(d)>=half)w=0;
      double h=d==0?2*fc:std::sin(2*M_PI*fc*d)/(M_PI*d);acc+=src(i)*h*w;wsum+=h*w;}
     double val=wsum!=0?acc/wsum:0;if(val>32767)val=32767;if(val<-32768)val=-32768;y[m]=int16_t(std::lround(val));
    }
    o.ratio=uint32_t(std::lround(r*65536));
    if(looped)o.type=(e.type&0xffff)|(Lk<<16);
   }
   for(int16_t s:y){fputc(uint16_t(s)>>8,vp);fputc(uint16_t(s)&255,vp);}
   o.count=uint32_t(y.size());voff+=o.count*2;vbytes+=o.count*2;var.push_back(o);
  }
 }
 fclose(vp);
 snprintf(p,sizeof p,"%s/voices_v.idx",dir);FILE*vi=fopen(p,"wb");assert(vi);
 be32(vi,0x58325633);be32(vi,n);for(auto&o:var){be32(vi,o.offset);be32(vi,o.count);be32(vi,o.ratio);be32(vi,o.type);}fclose(vi);
 printf("Variants: %llu bytes\n",(unsigned long long)vbytes);
 printf("Trimmed %u looped voices: %llu -> %llu bytes\n",shortened,(unsigned long long)before,(unsigned long long)after);
}
