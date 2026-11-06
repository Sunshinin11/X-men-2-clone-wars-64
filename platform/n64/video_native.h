#pragma once
// The game retains its original packed asset commands. On N64 these names
// dispatch directly to asset storage and RDP frame configuration, without the
// Genesis device, raster clock, FIFO, event recorder or software compositor.
#define gvdp_init x2_video_init
#define gvdp_screen_width x2_video_width
#define gvdp_screen_height x2_video_height
#define gvdp_output_height x2_video_output_height
#define gvdp_display_enabled x2_video_enabled
#define gvdp_interlace_double x2_video_double_height
#define gvdp_write_control x2_video_command
#define gvdp_write_data x2_video_upload
#define gvdp_read_data x2_video_read
#define gvdp_read_control x2_video_status
#define gvdp_store_vram_word x2_video_tile_word
#define gvdp_store_cram x2_video_palette_word
#define gvdp_store_vsram x2_video_scroll_word
#define gvdp_sat_byte x2_video_sprite_byte
#ifdef __cplusplus
extern "C" {
#endif
#include "genesis_vdp.h"
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern uint64_t x2_video_dirty_pages;
#ifdef __cplusplus
}
#endif
