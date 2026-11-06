#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "page_commands.hpp"
#include "sprite_scene.hpp"
#ifdef X2_TEST_RDP_DEVICE
extern "C" void x2_native_effects_check(GVDP*,const uint16_t*,const uint16_t*);
#endif
extern "C" void x2_scene_check(GVDP*v,const uint16_t*pal,const uint16_t*reference){
#ifdef X2_TEST_RDP_DEVICE
 x2_native_effects_check(v,pal,reference);return;
#endif
 if(!x2_scene_can(v))return;
 memset(x2_pattern_state,0,2048);
 static uint8_t low[320*224],high[320*224];static uint16_t out[320*240];
 for(int y=0;y<224;y++)x2_sprite_line(v,y,low+y*320,high+y*320);
 for(int i=0;i<320*240;i++)out[i]=i<320*224?pal[v->reg[7]&63]:1;
 static X2PageBatch batch;unsigned active_page=0,uploads=0,rects=0;
 auto upload=[&](unsigned p){assert(p<64);active_page=p;uploads++;};
 auto draw=[&](const X2Tile&t){X2RectWords r=x2_rect_words(t);
  int x0=((r.w[1]>>12)&4095)/4,y0=(r.w[1]&4095)/4;
  int width=((r.w[0]>>12)&4095)/4-x0,height=(r.w[0]&4095)/4-y0;
  int sx=(int16_t)(r.w[2]>>16)/32,sy=(int16_t)r.w[2]/32;
  int dx=(int16_t)(r.w[3]>>16)/1024,dy=(int16_t)r.w[3]/1024;unsigned p=(r.w[1]>>24)&7;
  assert(p<4&&width==t.w&&height==t.h&&x0==t.x&&y0==t.y&&(dx==1||dx==-1)&&(dy==1||dy==-1));
  for(int y=0;y<height;y++)for(int x=0;x<width;x++){int u=sx+x*dx,vv=sy+y*dy;assert(u>=0&&u<8&&vv>=0&&vv<256);
   unsigned nib=(v->vram[active_page*1024+vv*4+u/2]>>((u&1)?0:4))&15;
   if(nib)out[(y0+y)*320+x0+x]=pal[p*16+nib];}
  rects++;
 };
 static X2DualBatch dual[2];dual[0].init();dual[1].init();for(int i=0;i<2;i++){
 if(getenv("X2_REPRO_DISCARD")&&!i&&((v->reg[17]&31)||(v->reg[18]&31)))x2_plane_dual(v,i,[&](const X2Tile&t){dual[i].add_fallback(0,t);},[&](const X2Tile&t){dual[i].add_fallback(1,t);});
 else if(x2_plane_grid(v,i))x2_plane_batch(v,i,dual[i]);
 }
 auto compact_plane=[&](int which,int pri){dual[which].flush_pri(pri,upload,[&](const X2BatchNode&n,const X2ColData&c,const X2RowData&r){ X2Tile t={(int16_t)(c.x14>>14),(int16_t)(r.y2>>2),(uint8_t)((c.xw14-c.x14)>>14),(uint8_t)((r.yh2-r.y2)>>2),c.sx,r.sy,n.attr};draw(t);});};
 auto old_plane=[&](int which,int pri){batch.reset();auto emit=[&](const X2Tile&t){if(batch.count==X2PageBatch::CAPACITY)batch.flush(upload,draw);batch.add(t);};x2_plane(v,which,pri,emit);batch.flush(upload,draw);};
 auto plane=[&](int which,int pri){if(x2_plane_grid(v,which)||getenv("X2_REPRO_DISCARD"))compact_plane(which,pri);else old_plane(which,pri);};
 static unsigned calls;calls++;
 if(calls==120||calls==300||calls==600){char name[100];snprintf(name,sizeof name,"build/n64/hostcheck/snow-%u.state",calls);FILE*f=fopen(name,"wb");if(f){fwrite(v,sizeof(*v),1,f);fwrite(pal,256*2,1,f);fwrite(reference,320*240*2,1,f);fclose(f);}}
 for(int pri=0;pri<2;pri++){plane(1,pri);plane(0,pri);if(getenv("X2_NATIVE_SPRITES")){
 X2SpriteDesc sprites[80];int n=x2_sprite_extract(v,sprites,80);
 for(int k=n-1;k>=0;k--)if(sprites[k].pri==pri)x2_sprite_emit_tiles(sprites[k],[&](const X2Tile&t){active_page=(t.attr&2047)>>5;draw(t);});
 }else{uint8_t*spr=pri?high:low;for(int i=0;i<320*224;i++)if(spr[i]&15)out[i]=pal[spr[i]];}}
 assert(uploads<=rects);
 for(int i=0;i<320*240;i++)if((out[i]&65534)!=(reference[i]&65534)){fprintf(stderr,"SCENE MISMATCH x=%d y=%d actual=%x reference=%x\n",i%320,i/320,out[i],reference[i]);abort();}
 static unsigned checked,total_pages,total_rects;total_pages+=uploads;total_rects+=rects;if(!(++checked%60)){fprintf(stderr,"SCENE PASS frames=%u page_uploads=%u rectangles=%u\n",checked,total_pages,total_rects);total_pages=total_rects=0;}
}
