#include <libdragon.h>
#include <type_traits>
#include "page_commands.hpp"
#include "sprite_scene.hpp"
#include "operator_masks.hpp"
#include "native_sprites.hpp"

extern "C" void __rdpq_texture_rectangle(uint32_t,uint32_t,uint32_t,uint32_t);
static X2DualBatch batch_b, batch_a;
static X2ComplexBatch complex_batches[2];

#ifdef N64
extern "C" void x2_service_audio();
static inline void service_audio(){x2_service_audio();}
#else
static inline void service_audio(){}
#endif

static void finish_render(){
 rdpq_detach_wait();
}

static void render_audio(){service_audio();}

#ifndef X2_PLANE_CMD_CACHE
#define X2_PLANE_CMD_CACHE 1
#endif

#if defined(N64) && X2_PRESENT_VBLANKS == 1
#define X2_PLANE_DMA 1
extern "C" void __rdpq_autosync_use(uint32_t);
static uint32_t cached_plane_cmds[2][X2DualBatch::CAPACITY + 2 * 64 * 2][4] __attribute__((aligned(16)));

struct X2CachedPlane {
 const GVDP* video=nullptr;
 uint64_t dependencies=0;
 uint32_t key=0;
 uint16_t off[2]={0,0};
 uint16_t cmd_count[2]={0,0};
 uint16_t rect_count[2]={0,0};
 uint16_t page_count[2]={0,0};
};
static X2CachedPlane plane_cache[2];
#endif

static void prepare_planes(GVDP*v,uint64_t dirty){
#if !X2_PLANE_DMA
 (void)dirty;
#endif
 for(unsigned plane=0;plane<2;plane++){
  if(!gvdp_display_enabled(v)){
#if X2_PLANE_DMA
   if(plane_cache[plane].dependencies&dirty)plane_cache[plane].video=nullptr;
#endif
   continue;
  }
  auto& batch=plane?batch_b:batch_a;
  if(x2_plane_grid(v,plane)){
#if X2_PLANE_DMA
   unsigned hbase=(v->reg[13]&63)<<10;
   unsigned hs=x2_word(v,hbase+plane*2)&1023,vs=v->vsram[plane]&1023;
   unsigned table=plane?v->reg[4]&7:(v->reg[2]&56)>>3;
   uint32_t key=hs|(vs<<10)|(table<<20)|(uint32_t(v->reg[16])<<23);
   auto& saved=plane_cache[plane];
   if(saved.video==v&&saved.key==key&&!(saved.dependencies&dirty)){
    continue;
   }
   saved={v,0,key};
   batch.init(true);
   x2_plane_batch(v,plane,batch,&saved.dependencies);
   static const uint32_t k_step[4] = { 0x04000400, 0xFC000400, 0x0400FC00, 0xFC00FC00 };
   unsigned total_cmds = 0;
   auto* dst = cached_plane_cmds[plane];
   for(int pri=0; pri<2; pri++){
    saved.off[pri] = (uint16_t)total_cmds;
    unsigned pri_rects = 0, pri_pages = 0;
    uint64_t mask = batch.active_mask[pri];
    while(mask){
     unsigned p = __builtin_ctzll(mask);
     const uint8_t* source = v->vram + p * 1024;
     auto& image = dst[total_cmds++];
     image[0] = 0x27000000u; image[1] = 0;
     image[2] = 0x3d000000u | (FMT_I8 << 19) | 3; image[3] = PhysicalAddr(source);
     auto& load = dst[total_cmds++];
     load[0] = 0x26000000u; load[1] = 0;
     load[2] = 0x34000000u; load[3] = (TILE6 << 24) | (3 << 14) | (255 << 2);
     pri_pages++;
     for(unsigned i = batch.head[pri][p]; i != X2DualBatch::END; i = batch.nodes[i].next){
      const auto& n = batch.nodes[i];
      const auto& cd0 = batch.col_data[n.c];
      const uint32_t xw14 = batch.col_data[n.last_col].xw14;
      const auto& rd = batch.row_data[n.r];
      unsigned pal_idx = ((uint32_t)n.attr >> 13) & 3;
      int s = (n.attr & 2048) ? (7 - cd0.sx) : cd0.sx;
      int srow = (n.attr & 4096) ? (7 - rd.sy) : rd.sy;
      int texy = ((n.attr & 31) << 3) + srow;
      auto& cmd = dst[total_cmds++];
      cmd[0] = 0x24000000u | xw14 | rd.yh2;
      cmd[1] = (pal_idx << 24) | cd0.x14 | rd.y2;
      cmd[2] = ((uint32_t)s << 21) | ((uint32_t)texy << 5);
      cmd[3] = k_step[(n.attr >> 11) & 3];
      pri_rects++;
     }
     mask &= mask - 1;
    }
    saved.cmd_count[pri] = (uint16_t)(total_cmds - saved.off[pri]);
    saved.rect_count[pri] = (uint16_t)pri_rects;
    saved.page_count[pri] = (uint16_t)pri_pages;
   }
   if(total_cmds)data_cache_hit_writeback(dst, total_cmds * sizeof(*dst));
#else
   batch.init(true);
   x2_plane_batch(v,plane,batch);
#endif
  }else{
#if X2_PLANE_DMA
   plane_cache[plane].video=nullptr;
#endif
   auto&b=complex_batches[plane];b.reset();
   x2_plane_dual(v,plane,[&](const X2Tile&t){if(!x2_tile_blank(v,t.attr))b.add(0,t);},
                        [&](const X2Tile&t){if(!x2_tile_blank(v,t.attr))b.add(1,t);});
  }
 }
}

static uint16_t material_tlut[5][2][2][64] __attribute__((aligned(16)));
static X2NativeSprites native_sprites;

static bool draw_scene(surface_t*fb,GVDP*v,const uint16_t*pal,unsigned material,int clip_x0=0,int clip_y0=0,int clip_x1=320,int clip_y1=224){
 if(!gvdp_display_enabled(v)){
  rdpq_attach(fb,NULL);
  rdpq_set_mode_fill(RGBA32(0,0,0,255));
  rdpq_fill_rectangle(0,0,320,240);
  finish_render();
  return true;
 }
 if(!x2_scene_can(v))return false;

 rdpq_attach(fb,NULL);
 rdpq_set_scissor(clip_x0,clip_y0,clip_x1,(clip_x0==0&&clip_y0==0&&clip_x1==320&&clip_y1==224)?240:clip_y1);
 rdpq_set_mode_fill(RGBA32(0,0,0,255));
 rdpq_fill_rectangle(0,0,320,240);
 unsigned bg=pal[(v->reg[7]&63)+((material==1||material==3)?64:0)];
 rdpq_set_mode_fill(RGBA32((uint8_t)(((bg>>11)&31)*255/31),(uint8_t)(((bg>>6)&31)*255/31),(uint8_t)(((bg>>1)&31)*255/31),255));
 rdpq_fill_rectangle(0,0,320,224);
 rdpq_set_mode_standard();
 rdpq_mode_filter(FILTER_POINT);
 rdpq_mode_dithering(DITHER_NONE_NONE);
 rdpq_mode_combiner(RDPQ_COMBINER_TEX);
 rdpq_mode_alphacompare(1);
 rdpq_mode_tlut(TLUT_RGBA16);

 const uint16_t* active_palette = nullptr;
 auto set_material=[&](unsigned priority,unsigned sprite){
  uint16_t* palette = material_tlut[material][priority][sprite];
  if (palette == active_palette) return;
  rdpq_tex_upload_tlut(palette,0,64);
  active_palette = palette;
 };
 set_material(0,0);
#if X2_PLANE_DMA
 rdpq_set_tile(TILE6,FMT_I8,0,8,nullptr);
 for(unsigned p=0;p<4;p++){
  rdpq_tileparms_t tile={};
  tile.palette=p;
  tile.s.mask=3;
  rdpq_set_tile((rdpq_tile_t)p,FMT_CI4,0,8,&tile);
  rdpq_set_tile_size((rdpq_tile_t)p,0,0,8,256);
 }
#endif
 static int cur_tmem_page = -1;
 const uint8_t*texture_base=v->vram;

 auto upload=[&](unsigned page){
#if X2_PLANE_DMA
  rdpq_set_texture_image_raw(0,PhysicalAddr(texture_base+page*1024),FMT_I8,4,256);
  rdpq_load_tile(TILE6,0,0,4,256);
#else
  surface_t tex=surface_make((void*)(texture_base+page*1024),FMT_CI4,8,256,4);
  rdpq_texparms_t parms={};parms.s.repeats=REPEAT_INFINITE;rdpq_tex_multi_begin();rdpq_tex_upload(TILE0,&tex,&parms);
  for(int p=1;p<4;p++){parms.palette=p;rdpq_tex_reuse((rdpq_tile_t)p,&parms);}rdpq_tex_multi_end();
#endif
  cur_tmem_page = (int)page;
 };
 static const uint32_t k_step[4] = { 0x04000400, 0xFC000400, 0x0400FC00, 0xFC00FC00 };
 const bool full_scene=clip_x0==0&&clip_y0==0&&clip_x1==320&&clip_y1==224;

 auto draw=[&](const X2Tile&t){
  if(t.x+t.w<=clip_x0||t.x>=clip_x1||t.y+t.h<=clip_y0||t.y>=clip_y1)return;
  X2RectWords r=x2_rect_words(t);__rdpq_texture_rectangle(r.w[0],r.w[1],r.w[2],r.w[3]);
 };
 auto draw_spr_tile=[&](const X2Tile&t,bool direct){
  if(t.x+t.w<=clip_x0||t.x>=clip_x1||t.y+t.h<=clip_y0||t.y>=clip_y1)return;
  int p = (t.attr & 2047) >> 5;
  int key = direct ? p : p + 64;
  if(key != cur_tmem_page){ texture_base = direct ? v->vram : native_sprites.pixels; upload(p); cur_tmem_page = key; }
  draw(t);
 };

 auto plane=[&](int which,int pri,X2DualBatch& compact){
  texture_base=v->vram;cur_tmem_page=-1;
  set_material(pri,0);
  if(x2_plane_grid(v,which)){
#if X2_PLANE_DMA && X2_PLANE_CMD_CACHE
   if(full_scene){
    const auto& saved = plane_cache[which];
    unsigned cnt = saved.cmd_count[pri];
    if(cnt){
     __rdpq_autosync_use(AUTOSYNC_PIPE|AUTOSYNC_TILES|AUTOSYNC_TMEMS);
     rdpq_exec(cached_plane_cmds[which] + saved.off[pri], cnt * sizeof(cached_plane_cmds[0][0]));
    }
   }else{
    uint64_t mask = compact.active_mask[pri];
    while(mask){
     unsigned p = __builtin_ctzll(mask);
     bool page_loaded = false;
     for(unsigned i = compact.head[pri][p]; i != X2DualBatch::END; i = compact.nodes[i].next){
      const auto& n = compact.nodes[i];
      X2ColData cd = compact.col_data[n.c];
      cd.xw14 = compact.col_data[n.last_col].xw14;
      const auto& rd = compact.row_data[n.r];
      if(cd.xw14<=unsigned(clip_x0)*16384||cd.x14>=unsigned(clip_x1)*16384||
         rd.yh2<=unsigned(clip_y0)*4||rd.y2>=unsigned(clip_y1)*4)continue;
      if(!page_loaded){
       upload(p);
       page_loaded = true;
      }
      unsigned pal_idx = ((uint32_t)n.attr >> 13) & 3;
      int s = (n.attr & 2048) ? (7 - cd.sx) : cd.sx;
      int srow = (n.attr & 4096) ? (7 - rd.sy) : rd.sy;
      int texy = ((n.attr & 31) << 3) + srow;
      __rdpq_texture_rectangle(cd.xw14 | rd.yh2, (pal_idx << 24) | cd.x14 | rd.y2,
                               ((uint32_t)s << 21) | ((uint32_t)texy << 5), k_step[(n.attr >> 11) & 3]);
     }
     mask &= mask - 1;
    }
   }
   return;
#endif
  }
  complex_batches[which].flush(pri,upload,draw);
 };

 // 1. Draw Plane B Priority 0
 plane(1,0,batch_b);
 // 2. Draw Plane A Priority 0
 plane(0,0,batch_a);render_audio();

 // 3. Draw Sprites Priority 0
 set_material(0,1);
 texture_base=native_sprites.pixels;cur_tmem_page=-1;
 for(int i=int(native_sprites.count)-1; i>=0; i--){
  if(!(native_sprites.cells[i].attr&0x8000))draw_spr_tile(native_sprites.cells[i],native_sprites.direct[i]);
 }
 render_audio();

 // 4. Draw Plane B Priority 1
 plane(1,1,batch_b);
 // 5. Draw Plane A Priority 1
 plane(0,1,batch_a);render_audio();

 // 6. Draw Sprites Priority 1
 set_material(1,1);
 texture_base=native_sprites.pixels;cur_tmem_page=-1;
 for(int i=int(native_sprites.count)-1; i>=0; i--){
  if(native_sprites.cells[i].attr&0x8000)draw_spr_tile(native_sprites.cells[i],native_sprites.direct[i]);
 }

 finish_render();
 return true;
}

extern "C" bool x2_rdp_draw(surface_t*fb,GVDP*v,const uint16_t*pal){
 bool effects=(v->reg[12]&8)!=0;
 static uint16_t previous_palette[192];static bool palette_valid=false,previous_effects=false;
 if(!palette_valid||previous_effects!=effects||memcmp(previous_palette,pal,sizeof previous_palette)){
  for(unsigned m=0;m<5;m++)for(unsigned priority=0;priority<2;priority++)for(unsigned sprite=0;sprite<2;sprite++){
   unsigned brightness=m==1?(priority?0:64):m==2?(priority?128:0):m==3?64:0;
   for(unsigned i=0;i<64;i++){
    bool visible=i<64&&(i&15);
    if(effects&&sprite&&(i==62||i==63))visible=false;
    material_tlut[m][priority][sprite][i]=visible?pal[i+brightness]:0;
   }
  }
  data_cache_hit_writeback(material_tlut,sizeof material_tlut);
  memcpy(previous_palette,pal,sizeof previous_palette);previous_effects=effects;palette_valid=true;
 }
 if(gvdp_display_enabled(v)){native_sprites.build(v);}
 else native_sprites.count=0;
 if(native_sprites.packed)data_cache_hit_writeback(native_sprites.pixels,native_sprites.packed*32);

#ifdef N64
 const uint64_t dirty_pages=x2_video_dirty_pages;
 uint64_t dirty=dirty_pages;
 while(dirty){unsigned page=__builtin_ctzll(dirty);data_cache_hit_writeback(v->vram+page*1024,1024);dirty&=dirty-1;}
 x2_video_dirty_pages=0;
#else
 const uint64_t dirty_pages=~uint64_t(0);
 data_cache_hit_writeback(v->vram,sizeof v->vram);
#endif
 prepare_planes(v,dirty_pages);
 render_audio();

 if(!draw_scene(fb,v,pal,effects?1:0))return false;
 if(!effects||!gvdp_display_enabled(v)){
  return true;
 }
 auto&operators=native_sprites.operators;
 if(!operators.any(0)&&!operators.any(1)&&!operators.any(2)){
  return true;
 }
 static surface_t alternate=surface_alloc(FMT_RGBA16,320,240);
 for(unsigned which=0;which<3;which++){
  if(!operators.any(which))continue;
  int x0=320,y0=224,x1=0,y1=0;
  for(unsigned y=0;y<28;y++)for(unsigned x=0;x<40;x++)if(operators.tiles[which][y*40+x]){
   if(int(x*8)<x0)x0=x*8;
   if(int(y*8)<y0)y0=y*8;
   if(int(x*8+8)>x1)x1=x*8+8;
   if(int(y*8+8)>y1)y1=y*8+8;
  }
  draw_scene(&alternate,v,pal,which==0?2:which==1?3:4,x0,y0,x1,y1);
  data_cache_hit_writeback(operators.mask[which],sizeof operators.mask[which]);
  surface_t mask=surface_make(operators.mask[which],FMT_IA8,320,224,320);
  rdpq_attach(fb,NULL);
  rdpq_set_mode_standard();rdpq_mode_filter(FILTER_POINT);rdpq_mode_dithering(DITHER_NONE_NONE);
  rdpq_mode_tlut(TLUT_NONE);rdpq_mode_alphacompare(1);
  rdpq_mode_combiner(RDPQ_COMBINER2((0,0,0,TEX0),(0,0,0,TEX1),(0,0,0,COMBINED),(0,0,0,COMBINED)));
  for(unsigned y=0;y<28;y++)for(unsigned x=0;x<40;x++)if(operators.tiles[which][y*40+x]){
   surface_t color=surface_make_sub(&alternate,x*8,y*8,8,8);
   surface_t alpha=surface_make_sub(&mask,x*8,y*8,8,8);
   rdpq_tex_multi_begin();rdpq_tex_upload(TILE0,&color,NULL);rdpq_tex_upload(TILE1,&alpha,NULL);rdpq_tex_multi_end();
   rdpq_texture_rectangle(TILE0,x*8,y*8,x*8+8,y*8+8,0,0);
  }
  finish_render();
 }
 return true;
}
