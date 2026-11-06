#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstdarg>
struct surface_t{void*buffer;};
struct joypad_buttons_t{unsigned a,b,c_right,start,d_up,d_down,d_left,d_right;};
typedef int joypad_port_t;
#define assertf(c,...) assert(c)
#define RESOLUTION_320x240 0
#define DEPTH_16_BPP 0
#define GAMMA_NONE 0
#define FILTERS_RESAMPLE 0
#define DFS_DEFAULT_LOCATION 0
static void debug_init_isviewer(){}static void debug_init_usblog(){}
#define debugf(...) fprintf(stderr,__VA_ARGS__)
static FILE*romfile;
static void dfs_init(int){}static int dfs_open(const char*){romfile=fopen("game/xmen2.bin","rb");return romfile?1:-1;}
static int dfs_read(void*p,int s,int n,int){return fread(p,s,n,romfile);}static void dfs_close(int){fclose(romfile);}
static unsigned host_frames;
static unsigned host_limit(){const char*s=getenv("X2_HOST_FRAMES");return s?(unsigned)strtoul(s,nullptr,10):600;}
static void joypad_init(){}static void joypad_poll(){}static joypad_buttons_t joypad_get_buttons_held(int port){joypad_buttons_t b={};if(!port&&host_frames>=120){b.d_right=1;b.a=host_frames>=300&&host_frames<330;}return b;}
static void display_init(int,int,int,int,int){}
static uint16_t pixels[320*240];static surface_t surface={pixels};
static surface_t*display_get(){return &surface;}
static void display_show(surface_t*){if(++host_frames==host_limit()){FILE*out=fopen("build/n64/hostcheck/frame.ppm","wb");fprintf(out,"P6\n320 240\n255\n");for(auto p:pixels){unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>6)&31)*255/31),(unsigned char)(((p>>1)&31)*255/31)};fwrite(rgb,1,3,out);}fclose(out);fprintf(stderr,"HOSTCHECK %u frames completed, right and A input\n",host_frames);exit(0);}}
