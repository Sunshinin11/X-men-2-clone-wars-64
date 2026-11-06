#pragma once
#include <stdint.h>
struct X2SoundStats {uint32_t frames=0,samples=0,nonzero=0,peak=0,fm_writes=0,psg_writes=0,keyons=0,z80_pc=0,active_fm=0,active_psg=0;};
void x2_sound_init(unsigned rate);
uint8_t x2_sound_read(unsigned a);
void x2_sound_write(unsigned a,uint8_t value);
void x2_sound_busreq(bool req);
uint8_t x2_sound_busstatus();
void x2_sound_reset(bool run);
unsigned x2_sound_frame(int16_t*out,unsigned cap);
X2SoundStats x2_sound_stats();

struct X2AudioRuntimeStats { uint32_t pumps=0, max_gap_ticks=0, dma_bytes=0, dma_wait_ticks=0, max_dma_wait_ticks=0; uint32_t queue_checks=0, queue_empty=0, queue_low=0; };
X2AudioRuntimeStats x2_sound_runtime_stats();
