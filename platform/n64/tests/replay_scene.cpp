#include <cstdio>
#include <cstdlib>
#include "../page_commands.hpp"
extern "C" void x2_scene_check(GVDP*,const uint16_t*,const uint16_t*);
int main(int argc,char**argv){
 if(argc<2)return 2;
 for(int i=1;i<argc;i++){GVDP v;uint16_t pal[256],ref[320*240];FILE*f=fopen(argv[i],"rb");if(!f)return 3;
 if(fread(&v,sizeof v,1,f)!=1||fread(pal,sizeof pal,1,f)!=1||fread(ref,sizeof ref,1,f)!=1)return 4;fclose(f);
 for(int p=0;p<2;p++){unsigned total=0;x2_plane_dual(&v,p,[&](const X2Tile&){total++;},[&](const X2Tile&){total++;});static X2DualBatch b;b.init();if(!p&&((v.reg[17]&31)||(v.reg[18]&31)))x2_plane_dual(&v,p,[&](const X2Tile&t){b.add_fallback(0,t);},[&](const X2Tile&t){b.add_fallback(1,t);});else x2_plane_batch(&v,p,b);fprintf(stderr,"PLANE %d grid=%d expected=%u legacy_kept=%u window=%02x,%02x\n",p,x2_plane_grid(&v,p),total,b.count,v.reg[17],v.reg[18]);}
 x2_scene_check(&v,pal,ref);fprintf(stderr,"REAL STATE PASS %s scroll_mode=%u\n",argv[i],v.reg[11]);}
}
