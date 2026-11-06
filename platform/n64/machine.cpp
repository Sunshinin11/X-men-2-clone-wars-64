#include "audio_n64.hpp"
#include "native_pacing.hpp"
#ifndef X2_PRESENT_VBLANKS
#define X2_PRESENT_VBLANKS 1
#endif
static_assert(X2_PRESENT_VBLANKS == 1 || X2_PRESENT_VBLANKS == 2);
#ifndef X2_AUDIO_MODE
#define X2_AUDIO_MODE 3
#endif
#if X2_AUDIO_MODE < 0 || X2_AUDIO_MODE > 3
#error "Invalid audio mode"
#endif
X2AudioSystem g_x2_audio;
#include <libdragon.h>
#include <stdint.h>
#include <string.h>
extern "C" {
#include "genesis_vdp.h"
#include "genesis_dac.h"
extern unsigned char g_rom[0x200000];
uint8_t xm_ram_peek8(uint32_t);
void xm_native_mem_init();
}
extern "C" { unsigned long g_snd_frame=0; }
#include "xmen2.h"
namespace xm { extern uint32_t g_d0;
#ifdef X2_TEST_RDP_DEVICE
void fn_validate_tables();
#endif
}
static GVDP video __attribute__((aligned(16)));
extern "C" bool x2_rdp_draw(surface_t*,GVDP*,const uint16_t*);
static uint16_t palette[256],sr=0x2700;
static uint8_t io[16]={0xa0},zram[8192];
static bool in_irq;
static bool native_audio_bus_held;
[[maybe_unused]] static unsigned frames;
static volatile uint32_t g_vblanks = 0;
static uint32_t g_logic_ticks = 0;
#ifdef N64
#include <vi.h>
static volatile bool observing_display;
static surface_t* volatile pending_surface;
static volatile uint32_t pending_vblank,last_scheduled_vblank;
static volatile uint32_t g_last_vblank_ticks;
static uint32_t g_last_present_ticks;

static void vblank_cb(void*) {
 ++g_vblanks;
 g_last_vblank_ticks=TICKS_READ();
 if(pending_surface&&x2_vblank_reached(g_vblanks,pending_vblank)){
  display_show(pending_surface);
  last_scheduled_vblank=g_vblanks;
  observing_display=true;
  pending_surface=nullptr;
 }
 static uint32_t origin;
 if(!observing_display)return;
 uint32_t now=*VI_ORIGIN;
 if(now!=origin){
  origin=now;
 }
}

extern "C" void x2_service_audio(){g_x2_audio.render_and_submit_emergency();}
extern "C" void x2_background_service(){
 if(native_audio_bus_held)return;
 static uint32_t last;uint32_t now=TICKS_READ();
 if(now-last >= TICKS_PER_SECOND/500){
#if X2_AUDIO_OPT
  if(g_last_present_ticks&&(now-g_last_present_ticks>TICKS_PER_SECOND/50))g_x2_audio.render_and_submit();
  else g_x2_audio.render_and_submit_emergency();
#else
  g_x2_audio.render_and_submit();
#endif
  last=TICKS_READ();
 }
}

static void poll_game_input(){
 joypad_poll();
}
#else
static void vblank_cb(void*) { g_vblanks++; }
static void poll_game_input(){}
#endif

static uint16_t bus(void*,uint32_t a){a&=0xffffff;uint16_t v=0;for(int k=0;k<2;k++){unsigned p=(a+k)&0xffffff;v=v<<8|(p<0x200000?g_rom[p]:p>=0xff0000?xm_ram_peek8(p):0xff);}return v;}
static void colour(void*,unsigned i,uint16_t c){if(i>=64)return;for(int k=0;k<3;k++){uint32_t rgb=genesis_dac_cram_to_argb(c,k);palette[i+k*64]=(((rgb>>19)&31)<<11)|(((rgb>>11)&31)<<6)|(((rgb>>3)&31)<<1)|1;}}
static void vint(){if(in_irq||!(video.reg[1]&32)||((sr>>8)&7)>=6)return;in_irq=true;g_logic_ticks++;unsigned old=sr;sr=(sr&~0x700)|0x600;auto mark=xm::frame_mark();xm::frame_alloc(26);uint32_t d0=xm::g_d0;Irq_VBlankVector();xm::g_d0=d0;xm::frame_release(mark);sr=old;in_irq=false;}

#ifdef N64
static void present(){
 g_x2_audio.render_and_submit_emergency();
 surface_t*fb=display_get();
 bool ok=x2_rdp_draw(fb,&video,palette);
 assertf(ok,"Unsupported native video mode r1=%02x r12=%02x",video.reg[1],video.reg[12]);
 g_x2_audio.render_and_submit_emergency();

 disable_interrupts();
 bool late;
 uint32_t target=x2_next_present_vblank(g_vblanks,last_scheduled_vblank,observing_display,late,X2_PRESENT_VBLANKS);
 pending_vblank=target;pending_surface=fb;enable_interrupts();

#if X2_AUDIO_OPT && X2_PACE_SLICE_OPT
 {
  static uint32_t q1_since=0;
  auto try_pace_mix=[&]()->bool{
   if(!pending_surface)return false;
   uint32_t now_p=TICKS_READ();
   uint32_t vb_now=g_vblanks,vb_t0=g_last_vblank_ticks;
   int32_t vb_rem=(int32_t)(target-vb_now);
   uint32_t elapsed_in_vb=now_p-vb_t0;
   uint32_t period=TICKS_PER_SECOND/60;
   uint32_t rem_ticks=vb_rem>0
    ?((uint32_t)vb_rem*period>elapsed_in_vb?(uint32_t)vb_rem*period-elapsed_in_vb:0u)
    :0u;
   int q=audio_get_queued_buffers();
   if(q<=0){
    q1_since=0;
    return g_x2_audio.service_if_below(0,0);
   }
   if(q==1){
    if(!q1_since)q1_since=now_p;
    if(rem_ticks>=TICKS_PER_SECOND/450)return g_x2_audio.service_if_below(1,128);
    if(rem_ticks>=TICKS_PER_SECOND/800)return g_x2_audio.service_if_below(1,64);
    if(now_p-q1_since>=TICKS_PER_SECOND/35)return g_x2_audio.service_if_below(1,64);
    return false;
   }
   q1_since=0;
   if(rem_ticks>=TICKS_PER_SECOND/420)return g_x2_audio.service_if_below(-1,128);
   if(rem_ticks>=TICKS_PER_SECOND/700)return g_x2_audio.service_if_below(-1,64);
   return false;
  };
  if(pending_surface){
   while(pending_surface&&try_pace_mix()){}
   uint32_t last_pace_poll=TICKS_READ();
   while(pending_surface){
    uint32_t now_p=TICKS_READ();
    if(now_p-last_pace_poll>=TICKS_PER_SECOND/5000){
     last_pace_poll=now_p;
     while(pending_surface&&try_pace_mix()){}
    }
   }
  }
 }
#elif X2_AUDIO_OPT
 {
  static bool pace_deferred_mix=false;
  auto try_pace_mix=[&](bool initial){
   uint32_t now_p=TICKS_READ();
   uint32_t vb_now=g_vblanks,vb_t0=g_last_vblank_ticks;
   int32_t vb_rem=(int32_t)(target-vb_now);
   uint32_t elapsed_in_vb=now_p-vb_t0;
   uint32_t period=TICKS_PER_SECOND/60;
   uint32_t rem_ticks=vb_rem>0
    ?((uint32_t)vb_rem*period>elapsed_in_vb?(uint32_t)vb_rem*period-elapsed_in_vb:0u)
    :0u;
   uint32_t min_window=initial?(TICKS_PER_SECOND/270):(TICKS_PER_SECOND/220);
   if(rem_ticks>=min_window||pace_deferred_mix){
    if(g_x2_audio.service_if_below(-1))pace_deferred_mix=false;
    else if(audio_get_queued_buffers()>=3)pace_deferred_mix=false;
   }else{
    g_x2_audio.service_if_below(1);
    if(audio_get_queued_buffers()==2)pace_deferred_mix=true;
   }
  };
  if(pending_surface){
   try_pace_mix(true);
   uint32_t last_pace_poll=TICKS_READ();
   while(pending_surface){
    uint32_t now_p=TICKS_READ();
    if(now_p-last_pace_poll>=TICKS_PER_SECOND/5000){
     last_pace_poll=now_p;
     try_pace_mix(false);
    }
   }
  }
 }
#else
 while(pending_surface)g_x2_audio.render_and_submit();
#endif
 g_last_present_ticks=TICKS_READ();
}
#else
static void present(){
 g_x2_audio.render_and_submit();
 surface_t*fb=display_get();
 bool native=x2_rdp_draw(fb,&video,palette);
 (void)native;
 display_show(fb);
 g_x2_audio.render_and_submit();
}
#endif

extern "C" {
XmOptions g_opt={};
void plat_sync_point(){if(in_irq)return;poll_game_input();vint();present();}
void plat_wait_vint(){if(!in_irq){
 uint32_t vb=g_vblanks;
#if X2_AUDIO_OPT && defined(N64)
 g_x2_audio.render_and_submit();
 uint32_t last_vint_poll=TICKS_READ();
 while(g_vblanks==vb){
  uint32_t now_v=TICKS_READ();
  if(now_v-last_vint_poll>=TICKS_PER_SECOND/5000){
   last_vint_poll=now_v;
   g_x2_audio.render_and_submit();
  }
 }
#else
 while(g_vblanks==vb)g_x2_audio.render_and_submit();
#endif
 poll_game_input();vint();}}
unsigned short plat_sr(){return sr;}void plat_set_sr(unsigned short v){sr=v;}
uint16_t plat_vdp_data_read16(){return gvdp_read_data(&video);}uint8_t plat_vdp_data_read8(){return plat_vdp_data_read16()>>8;}uint32_t plat_vdp_data_read32(){uint32_t h=plat_vdp_data_read16();return h<<16|plat_vdp_data_read16();}
void plat_vdp_data_write16(uint16_t v){gvdp_write_data(&video,v);}void plat_vdp_data_write8(uint8_t v){plat_vdp_data_write16(v*257);}void plat_vdp_data_write32(uint32_t v){plat_vdp_data_write16(v>>16);plat_vdp_data_write16(v);}
uint16_t plat_vdp_status16(){video.in_vblank^=1;return gvdp_read_control(&video);}uint32_t plat_vdp_status32(){uint32_t h=plat_vdp_status16();return h<<16|plat_vdp_status16();}
void plat_vdp_ctrl_write16(uint16_t v){gvdp_write_control(&video,v);}void plat_vdp_ctrl_write32(uint32_t v){plat_vdp_ctrl_write16(v>>16);plat_vdp_ctrl_write16(v);}uint8_t plat_vdp_hv8(){return 0;}uint16_t plat_vdp_hv16(){return 0;}
void plat_psg_write8(uint8_t v){(void)v;}
uint8_t plat_z80_busreq_read8(){return X2_AUDIO_MODE>=2?x2_sound_busstatus():0;}
uint16_t plat_z80_busreq_read16(){return X2_AUDIO_MODE>=2?(uint16_t)x2_sound_busstatus()<<8:0;}
void plat_z80_busreq_write8(uint8_t v){native_audio_bus_held=(v&1)!=0;if(X2_AUDIO_MODE>=2)x2_sound_busreq(v&1);}
void plat_z80_busreq_write16(uint16_t v){native_audio_bus_held=(v&0x100)!=0;if(X2_AUDIO_MODE>=2)x2_sound_busreq(v&0x100);}
uint16_t plat_z80_reset_read16(){return 0;}
void plat_z80_reset_write16(uint16_t v){if(X2_AUDIO_MODE>=2)x2_sound_reset(v&0x100);}
uint8_t plat_z80_ram_read8(uint32_t a){return X2_AUDIO_MODE>=2?x2_sound_read(a):zram[a&8191];}
void plat_z80_ram_write8(uint32_t a,uint8_t v){
 if(X2_AUDIO_MODE>=2)x2_sound_write(a,v);
 zram[a&8191]=v;
 if((a&8191)==0x36){
  g_x2_audio.process_ring(zram,v);
 }
}
void plat_tmss_write32(uint32_t){}
uint8_t plat_io_read8(int r){
 r&=15;
 if(r==1||r==2){auto b=joypad_get_buttons_held((joypad_port_t)(r-1));uint8_t v=0x7f;bool th=io[r]&64;if(b.d_up)v&=~1;if(b.d_down)v&=~2;if(th){if(b.d_left)v&=~4;if(b.d_right)v&=~8;if(b.b)v&=~16;if(b.c_right)v&=~32;}else{v&=~12;if(b.a)v&=~16;if(b.start)v&=~32;}return(v&~io[r+3])|(io[r]&io[r+3]);}
 return io[r];
}
void plat_io_write8(int r,uint8_t v){io[r&15]=v;}uint16_t plat_io_ctrl12_read16(){return io[4]*257;}uint32_t plat_io_ctrl12_read32(){return (uint32_t)io[4]*0x1010000|io[5]*257;}uint16_t plat_io_ctrl3_read16(){return io[6]*257;}
int plat_ws_view(){return 0;}int plat_ws_left(){return 0;}int plat_ws_right(){return 0;}int plat_ws_act(){return 0;}int plat_ws_hud(){return 0;}void plat_ws_cerebro(int){}void plat_spr_owner(const void*){}void plat_spr_anchor(const void*,long,long){}int plat_debug_first_scene(int s){
#ifndef N64
 const char* test_scene=getenv("X2_TEST_SCENE");
 if(test_scene)return atoi(test_scene);
#endif
 return s;
}void plat_p34_enter(int){}unsigned short plat_pad34(int){return 0;}int plat_p34_dma(unsigned long,unsigned short,unsigned short){return 0;}void plat_spr_dropped(int){}void plat_p34_hud(int,short,short,int,unsigned short){}void plat_hud_char(int,int){}

int main(){
 debug_init_isviewer();
 debug_init_usblog();
 dfs_init(DFS_DEFAULT_LOCATION);
 memset(g_rom,255,sizeof(g_rom));
 int f=dfs_open("xmen2.bin");
 assertf(f>=0,"missing game ROM");
 assertf(dfs_read(g_rom,1,0x200000,f)==0x200000,"short ROM");
 dfs_close(f);
 xm_native_mem_init();
 joypad_init();
#ifdef X2_TEST_RDP_DEVICE
 xm::fn_validate_tables();
#endif

#ifdef N64
 rdpq_init();
 vi_install_vblank_handler(vblank_cb, NULL);
#endif
 g_x2_audio.init(X2_AUDIO_MODE);
 display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
 gvdp_init(&video);
 video.bus_read=bus;
 video.colour_updated=colour;
 g_opt.players=2;
 EntryPoint();
 return 0;
}
}
