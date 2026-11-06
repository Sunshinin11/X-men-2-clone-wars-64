/* Retain the existing clean-room sprite evaluator and its priority/collision rules. */
#include "../../segagenesisrecomp/runner/video/genesis_vdp.c"
#ifndef XMEN2_N64_NATIVE_VIDEO
void x2_sprite_line(GVDP*v,int y,uint8_t*low,uint8_t*high){
 unsigned hbase=(v->reg[13]&63)<<10;int mode=v->reg[11]&3,hi=mode==0?0:mode==2?(y&~7):y;
 int hs=vram_read_word(v,(uint16_t)(hbase+hi*4+2))&1023;
 sprite_render_line(v,y,320,0,hs&15);
 for(int x=0;x<320;x++){low[x]=s_spr_op[x]&&!s_spr_hi[x]?s_spr_idx[x]:0;high[x]=s_spr_op[x]&&s_spr_hi[x]?s_spr_idx[x]:0;}
}

#endif


uint8_t x2_pattern_state[2048] __attribute__((weak));
