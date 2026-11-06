#include <libdragon.h>
#include "../scene.hpp"
#include <cstdio>
#include <cstdlib>
extern "C" bool x2_rdp_draw(surface_t*,GVDP*,const uint16_t*);
extern "C" void x2_native_effects_check(GVDP*v,const uint16_t*pal,const uint16_t*reference){
 static surface_t fb=surface_alloc(FMT_RGBA16,320,240);static unsigned frames;
 GVDP input=*v;
 if(!x2_rdp_draw(&fb,v,pal)){if(gvdp_display_enabled(v))abort();return;}
 for(unsigned i=0;i<320*240;i++)if((((uint16_t*)fb.buffer)[i]&65534)!=(reference[i]&65534)){
  FILE*f=fopen("build/n64/hostcheck/rdp-mismatch.state","wb");if(f){fwrite(&input,sizeof input,1,f);fwrite(pal,512,1,f);fwrite(reference,320*240*2,1,f);fclose(f);}
  fprintf(stderr,"PRODUCTION RDP MISMATCH frame=%u x=%u y=%u actual=%04x expected=%04x mode=%02x\n",frames,i%320,i/320,((uint16_t*)fb.buffer)[i],reference[i],v->reg[12]);abort();
 }
 if(!(++frames%60))fprintf(stderr,"PRODUCTION RDP PASS frames=%u shadow_highlight=%u\n",frames,(v->reg[12]&8)!=0);
}
