// Original sound-machine bus/timers adapted from platform/native/machine.cpp.
// Keeps the original ROM Z80 driver and the existing YM2612/PSG implementations.
#include "sound_core.hpp"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
extern "C" {
#include "external/superzazu/z80.h"
#include "audio/ym2612.h"
#include "audio/sn76489.h"
void x2_ym_rate(unsigned);
void x2_psg_rate(unsigned);
unsigned x2_psg_active(void);
extern uint8_t g_rom[0x200000];
uint8_t xm_ram_peek8(uint32_t);
}
namespace {
constexpr unsigned MPL=3420,LINES=262,MASTER_CLOCK=53693175;
struct SoundBus { uint8_t z80_ram[8192];uint16_t z80_bank;uint8_t z80_busreq,z80_reset_off;};
struct SoundState { struct z80 z80;SoundBus bus; } sound_state;
static uint32_t synth_time,output_rate=12000;
static uint64_t sample_phase;
static X2SoundStats stats;
static uint8_t fm_latch[2],fm_keys,psg_latch,psg_volume[4];
static uint8_t mem_peek8(uint32_t a){a&=0xffffff;return a<sizeof(g_rom)?g_rom[a]:a>=0xff0000?xm_ram_peek8(a):0xff;}
static void machine_fatal(const char*s){fprintf(stderr,"[X2_SOUND] %s\n",s);abort();}
static void synth_advance(uint32_t t){if(t>synth_time){ym2612_advance(t-synth_time);psg_advance(t-synth_time);synth_time=t;}}
static void synth_write(uint32_t t,unsigned port,uint8_t value){
 synth_advance(t);
 if(port==4){psg_write(value);stats.psg_writes++;
  if(value&128)psg_latch=(value>>4)&7;
  if(psg_latch&1)psg_volume[psg_latch>>1]=value&15;
 }
 else{ym2612_write(port,value);stats.fm_writes++;if(!(port&1))fm_latch[port>>1]=value;
 else if(port==1&&fm_latch[0]==0x28){unsigned ch=value&7;
  if((ch&3)<3){ch=(ch&3)+((ch&4)?3:0);if(value&0xf0){fm_keys|=1<<ch;stats.keyons++;}else fm_keys&=~(1<<ch);}
 }}
}
static uint8_t z80_bus_read(uint16_t addr);
static void    z80_bus_write(uint16_t addr, uint8_t val);
static uint8_t z80_read(void *u, uint16_t a)            { (void)u; return z80_bus_read(a); }
static void    z80_write(void *u, uint16_t a, uint8_t v) { (void)u; z80_bus_write(a, v); }
static uint8_t z80_port_in(z80 *z, uint8_t port) { return z80_bus_read((uint16_t)((z->b << 8) | port)); }
static void    z80_port_out(z80 *z, uint8_t port, uint8_t value) { z80_bus_write((uint16_t)((z->b << 8) | port), value); }

static void z80_own_reset(z80 *z)
{
    z->pc = 0;
    z->iff1 = z->iff2 = 0;
    z->halted = 0;
    z->interrupt_mode = 0;
    z->iff_delay = 0;
    z->int_pending = z->nmi_pending = 0;
    z->int_data = 0xFF;
}

/* ======================================================================== */
/* Z80 core in master clocks                                                */

#define MASTER_PER_Z80 15u
static uint32_t s_znow;          /* the current Z80 instruction's bus time (YM timers) */
static uint32_t s_zstamp;        /* audio stamp: frame-relative master clock           */
static uint32_t s_zclk;          /* Z80 time in master clocks                          */
static int      s_zstate;        /* bit0 reset released, bit1 bus requested            */
static int      s_zaccess;       /* the game may access the Z80 area (bus granted)     */
static int      ei_shadow;       /* the instruction after EI: no interrupt yet          */

static void z80_exact_step(uint32_t *zclk, uint32_t target)
{
    z80 *z = &sound_state.z80;
    while (*zclk < target) {
        s_zstamp = *zclk;
        if (!ei_shadow) {
            unsigned long c0 = z->cyc;
            if (z80_take_irq(z)) {
                *zclk += (uint32_t)(z->cyc - c0) * MASTER_PER_Z80;
                if (*zclk >= target) break;
            }
        }
        unsigned long c0 = z->cyc;
        s_znow = *zclk + z80_base_cycles(z) * MASTER_PER_Z80;
        ei_shadow = z80_exec_one(z);
        *zclk += (uint32_t)(z->cyc - c0) * MASTER_PER_Z80;
    }
}

static void z80_exact_reset(void)
{
    z80_own_reset(&sound_state.z80);
    sound_state.z80.i = 0;
    sound_state.z80.r = 0;
}

static void z80_exact_power_on(void)
{
    z80 *z = &sound_state.z80;
    z->a = z->b = z->c = z->d = z->e = z->h = z->l = 0;
    z->a_ = z->b_ = z->c_ = z->d_ = z->e_ = z->h_ = z->l_ = z->f_ = 0;
    z->sp = z->ix = z->iy = 0;
    z->mem_ptr = 0;
    z->sf = 0; z->zf = 1; z->yf = 0; z->hf = 0; z->xf = 0; z->pf = 0; z->nf = 0; z->cf = 0;
    z80_exact_reset();
}

static void z80_exact_irq(int asserted) { sound_state.z80.int_pending = asserted ? 1 : 0; }

/* ======================================================================== */
/* YM2612 timers and status (what the sound driver sees; the sound is the chips') */

#define FM_SAMPLE 1008u
static uint32_t s_fm_count, s_fm_start;
static int      s_fm_ta, s_fm_tal = 1024, s_fm_tac, s_fm_tbl = 256 << 4, s_fm_tbc;
static uint8_t  s_fm_mode, s_fm_status;
static uint16_t s_fm_addr;
/* the sound chips' registers as written (phase 7: the canonical digest has them in every run; the
   chips themselves run only in the session machine) */
static uint8_t  s_ym_reg[512], s_ym_sel[2];
static uint16_t s_psg_reg[8];
static uint8_t  s_psg_latch;

static void ym_shadow(unsigned port, uint8_t v)
{
    if (!(port & 1)) s_ym_sel[port >> 1 & 1] = v;
    else s_ym_reg[(port >> 1 & 1) << 8 | s_ym_sel[port >> 1 & 1]] = v;
}

static void psg_shadow(uint8_t v)
{
    if (v & 0x80) {
        s_psg_latch = (uint8_t)(v >> 4 & 7);
        s_psg_reg[s_psg_latch] = (uint16_t)((s_psg_reg[s_psg_latch] & 0x3F0) | (v & 0x0F));
    } else
        s_psg_reg[s_psg_latch] = (uint16_t)((s_psg_reg[s_psg_latch] & 0x0F) | (v & 0x3F) << 4);
}

static void fm_update(uint32_t t)
{
    if (t <= s_fm_count) return;
    uint32_t n = (t - s_fm_count + FM_SAMPLE - 1) / FM_SAMPLE;
    for (uint32_t i = 0; i < n; i++) {
        if (s_fm_mode & 0x01) {
            if (--s_fm_tac <= 0) {
                if (s_fm_mode & 0x04) s_fm_status |= 0x01;
                s_fm_tac = s_fm_tal;
            }
        }
    }
    if (s_fm_mode & 0x02) {
        s_fm_tbc -= (int)n;
        if (s_fm_tbc <= 0) {
            if (s_fm_mode & 0x08) s_fm_status |= 0x02;
            if (s_fm_tbl) s_fm_tbc += s_fm_tbl; else s_fm_tbc = s_fm_tbl;
        }
    }
    s_fm_count += n * FM_SAMPLE;
}

static void fm_set_timers(uint8_t v)
{
    if ((v & 1) && !(s_fm_mode & 1)) s_fm_tac = s_fm_tal;
    if ((v & 2) && !(s_fm_mode & 2)) s_fm_tbc = s_fm_tbl;
    s_fm_status &= (uint8_t)(~v >> 4);
    s_fm_mode = v;
}

static void fm_reset_chip(uint32_t t)
{
    fm_update(t);
    s_fm_tac = s_fm_tbc = 0;
    fm_set_timers(0x30);
    s_fm_tbl = 256 << 4;
    s_fm_ta = 0;
    s_fm_tal = 1024;
}

static uint8_t ym_read(uint32_t t)
{
    fm_update(t);
    return s_fm_status;
}

static void ym_write(uint32_t t, unsigned port, uint8_t v)
{
    if (port & 1) fm_update(t);
    switch (port & 3) {
    case 0: s_fm_addr = v; break;
    case 2: s_fm_addr = (uint16_t)(v | 0x100); break;
    default:
        switch (s_fm_addr) {
        case 0x24: s_fm_ta = (s_fm_ta & 0x03) | (v << 2); s_fm_tal = 1024 - s_fm_ta; break;
        case 0x25: s_fm_ta = (s_fm_ta & 0x3FC) | (v & 3); s_fm_tal = 1024 - s_fm_ta; break;
        case 0x26: s_fm_tbl = (256 - v) << 4; break;
        case 0x27: fm_set_timers(v); break;
        default: break;
        }
        break;
    }
}

/* ======================================================================== */
/* The Z80's bus (the sound driver's address space)                         */

static uint8_t z80_bus_read(uint16_t addr)
{
    SoundBus *b = &sound_state.bus;
    if (addr < 0x4000u) return b->z80_ram[addr & 0x1FFFu];
    if (addr < 0x6000u) return ym_read(s_znow);
    if (addr < 0x8000u) return 0xFFu;
    return mem_peek8(((uint32_t)b->z80_bank << 15) + (uint32_t)(addr - 0x8000u));   /* the bank window */
}

static void z80_bus_write(uint16_t addr, uint8_t val)
{
    SoundBus *b = &sound_state.bus;
    if (addr < 0x4000u) {
        b->z80_ram[addr & 0x1FFFu] = val;
        return;
    }
    if (addr < 0x6000u) {
        uint8_t port = (uint8_t)(addr & 3);
        ym_write(s_znow, port, val);
        ym_shadow(port, val);
        synth_write(s_zstamp,port,val);
        return;
    }
    if (addr < 0x6100u) { b->z80_bank = ((b->z80_bank >> 1) | ((val & 1) << 8)) & 0x1FF; return; }
    if (addr == 0x7F11u) {
        psg_shadow(val);
        synth_write(s_zstamp,4,val);
        return;
    }
    if (addr >= 0x8000u) machine_fatal("the Z80 wrote the 68000 bus through its bank window (not in the game)");
}

/* ======================================================================== */
/* Z80 control from the game: bus request, reset                            */

static void z80_busreq_w(int req)
{
    if (req) {
        if (s_zstate == 1) s_zaccess = 1;
        s_zstate |= 2;
    } else {
        if (s_zstate == 3) s_zaccess = 0;
        s_zstate &= 1;
        /* a released Z80 runs a fixed slice, so a game waiting for the sound driver's answer
           sees it after a deterministic number of polls */
        if (s_zstate == 1) z80_exact_step(&s_zclk, s_zclk + 8u * MPL);
    }
    sound_state.bus.z80_busreq = (uint8_t)((s_zstate & 2) ? 1 : 0);
}

static void z80_reset_w(int release)
{
    uint32_t now = s_zclk;
    if (release) {
        if (s_zstate == 0) { z80_exact_reset(); fm_reset_chip(now); }
        else if (s_zstate == 2) { s_zaccess = 1; z80_exact_reset(); fm_reset_chip(now); }
        s_zstate |= 1;
    } else {
        if (s_zstate == 3) s_zaccess = 0;
        fm_reset_chip(now);
        s_zstate &= 2;
    }
    sound_state.bus.z80_reset_off = (uint8_t)(s_zstate & 1);
}

}
void x2_sound_init(unsigned rate){
 memset(&sound_state,0,sizeof sound_state);stats={};fm_keys=0;psg_latch=0;memset(psg_volume,15,sizeof psg_volume);output_rate=rate;sample_phase=0;synth_time=0;
 z80_init(&sound_state.z80);z80_exact_power_on();
 sound_state.z80.read_byte=z80_read;sound_state.z80.write_byte=z80_write;
 sound_state.z80.port_in=z80_port_in;sound_state.z80.port_out=z80_port_out;
 s_zclk=0;s_zstate=0;s_zaccess=0;ei_shadow=0;s_fm_count=s_fm_start=0;
 s_fm_ta=0;s_fm_tal=1024;s_fm_tac=0;s_fm_tbl=256<<4;s_fm_tbc=0;s_fm_mode=0x30;s_fm_status=0;
 x2_ym_rate(rate);x2_psg_rate(rate);ym2612_init();psg_init();
}
uint8_t x2_sound_read(unsigned a){return sound_state.bus.z80_ram[a&8191];}
void x2_sound_write(unsigned a,uint8_t v){sound_state.bus.z80_ram[a&8191]=v;}
void x2_sound_busreq(bool req){z80_busreq_w(req);}
uint8_t x2_sound_busstatus(){return s_zaccess?0:1;}
void x2_sound_reset(bool run){z80_reset_w(run);}
X2SoundStats x2_sound_stats(){stats.active_fm=__builtin_popcount((unsigned)fm_keys);stats.active_psg=x2_psg_active();return stats;}
unsigned x2_sound_frame(int16_t*out,unsigned cap){
 constexpr uint32_t F=MPL*LINES;
 if(s_zstate==1){z80_exact_irq(1);z80_exact_step(&s_zclk,s_zclk+MPL);z80_exact_irq(0);}
 if(s_zstate==1&&s_zclk<F)z80_exact_step(&s_zclk,F);
 if(s_zclk<F)s_zclk=F;
 fm_update(F);
 uint32_t span=F>s_fm_start?F-s_fm_start:0,k=(span+FM_SAMPLE-1)/FM_SAMPLE;if(!k)k=1;
 s_fm_count=s_fm_start=s_fm_start+k*FM_SAMPLE-F;s_zclk-=F;
 synth_advance(F);
 static int16_t fm[4096*2],psg[8192];
 unsigned nf=ym2612_render(fm,4096),np=psg_render(psg,8192);
 unsigned count=0;
 // Sample-clock phase carries across frames; no fixed 200-sample/frame assumption.
 uint64_t end=(uint64_t)F*output_rate;
 while(sample_phase<end){
  unsigned fi=(unsigned)(sample_phase/MASTER_CLOCK);
  unsigned pi=(unsigned)(sample_phase/MASTER_CLOCK);
  if(fi>=nf)fi=nf?nf-1:0;if(pi>=np)pi=np?np-1:0;
  int p=np?psg[pi]:0;
  for(int ch=0;ch<2;ch++){int value=((nf?fm[fi*2+ch]:0)+p)/2;
   if(value>32767)value=32767;if(value< -32768)value=-32768;
   if(count<cap)out[count*2+ch]=(int16_t)value;
   if(value){stats.nonzero++;unsigned a=value<0?-value:value;if(a>stats.peak)stats.peak=a;}
  }
  count++;sample_phase+=MASTER_CLOCK;
 }
 sample_phase-=end;
 // Bus-grant slices can run ahead; keep their audio time rather than generating twice.
 synth_time=synth_time>F?synth_time-F:0;
 stats.frames++;stats.samples+=count;stats.z80_pc=sound_state.z80.pc;
 assert(count<=cap);return count;
}
