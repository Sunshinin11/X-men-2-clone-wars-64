#pragma once
#include "operator_masks.hpp"

// Native CI4 sprite packets. Visibility is resolved into texture alpha once;
// the RDP performs color lookup, rasterization and composition.
struct X2NativeSprites {
 alignas(16) uint8_t pixels[65536];
 X2Tile cells[1280];unsigned count=0;
 // direct[i]: the cell samples the game's own pattern in VRAM (flips in the
 // rectangle); otherwise it samples a resolved packet in pixels[].
 uint8_t direct[1280];unsigned packed=0;
 X2OperatorMasks operators;
 uint8_t row_ids[224][20],row_counts[224];
 void build(GVDP*v){
  X2SpriteDesc raw[80];unsigned xword[80],n=0,link=0;
  uint8_t allowed[80][32]={};
  unsigned sat=(v->reg[5]&127)<<9;
  for(unsigned step=0;step<80;step++){
   unsigned c=link*8;uint16_t attr=x2_word(v,sat+c+4);
   raw[n]={int16_t((x2_word(v,sat+c+6)&1023)-128),int16_t((((v->sat[c]<<8)|v->sat[c+1])&511)-128),
    uint8_t(((v->sat[c+2]>>2)&3)+1),uint8_t((v->sat[c+2]&3)+1),uint8_t((attr>>13)&3),uint8_t(attr>>15),uint8_t((attr>>11)&1),uint8_t((attr>>12)&1),uint16_t(attr&2047)};
   xword[n++]=x2_word(v,sat+c+6)&1023;
   link=v->sat[c+3]&127;if(!link||link>=80)break;
  }
  bool carry=v->spr_mask_carry!=0;
  memset(row_counts,0,sizeof row_counts);
  for(unsigned i=0;i<n;i++){
   int first=raw[i].y<0?0:raw[i].y,last=raw[i].y+raw[i].vsz*8;if(last>224)last=224;
   for(int y=first;y<last;y++)if(row_counts[y]<20)row_ids[y][row_counts[y]++]=i;
  }
  for(int y=0;y<224;y++){
   unsigned pixels_used=0;bool seen=carry,masked=false;carry=false;
   for(unsigned entry=0;entry<row_counts[y];entry++){
    unsigned i=row_ids[y][entry];auto&s=raw[i];int row=y-s.y;
    if(xword[i]&511)seen=true;else if(seen)masked=true;
    unsigned width=s.hsz*8;
    unsigned available=pixels_used<320?320-pixels_used:0;
    unsigned visible=width<available?width:(available&~7u);
    pixels_used+=width;
    if(!masked&&s.x+int(width)>0&&s.x<320)allowed[i][row]=visible;
    if(pixels_used>=320){carry=true;break;}
   }
  }
  v->spr_mask_carry=carry;
  bool effects=(v->reg[12]&8)!=0;
  if(!effects){build_direct(v,raw,n,allowed);return;}
  if(effects)memset(&operators,0,sizeof operators);
  else memset(operators.occupied,0,sizeof operators.occupied);
  count=0;packed=0;
  for(unsigned i=0;i<n;i++)x2_sprite_emit_tiles(raw[i],[&](const X2Tile&t){
   assert(count<1280);uint8_t*dst=pixels+count*32;memset(dst,0,32);bool any=false;
   for(unsigned dy=0;dy<t.h;dy++)for(unsigned dx=0;dx<t.w;dx++){
    unsigned x=t.x+dx,y=t.y+dy,p=y*320+x;
    unsigned local_x=int(x)-raw[i].x,local_y=int(y)-raw[i].y;
    if(local_x>=allowed[i][local_y]||(operators.occupied[p>>3]&(1u<<(p&7))))continue;
    unsigned sx=t.sx+dx,sy=t.sy+dy;if(t.attr&2048)sx=7-sx;if(t.attr&4096)sy=7-sy;
    unsigned a=(t.attr&2047)*32+sy*4+sx/2;
    unsigned nib=(v->vram[a]>>((sx&1)?0:4))&15;if(!nib)continue;
    if(effects&&((t.attr>>13)&3)==3&&nib>=14){
     unsigned which=nib-14;operators.mask[which][p]=255;operators.tiles[which][(y/8)*40+x/8]=1;
    }else{
     operators.occupied[p>>3]|=1u<<(p&7);dst[dy*4+dx/2]|=nib<<((dx&1)?0:4);any=true;
    }
   }
   if(any){cells[count]={t.x,t.y,t.w,t.h,0,0,uint16_t((t.attr&0xe000)|count)};direct[count]=0;count++;}
  });
  packed=count;
  if(effects)for(unsigned p=0;p<320*224;p++)if(operators.mask[0][p]&&operators.mask[1][p]){
   operators.mask[0][p]=operators.mask[1][p]=0;operators.mask[2][p]=255;operators.tiles[2][(p/320/8)*40+(p%320)/8]=1;
  }
 }

 // Pattern nibble of sprite s at screen (x,y), honouring its line budget.
 static unsigned pixel_at(const GVDP*v,const X2SpriteDesc&s,const uint8_t*allow,int x,int y){
  int lx=x-s.x,ly=y-s.y;
  if(lx<0||ly<0||lx>=s.hsz*8||ly>=s.vsz*8||unsigned(lx)>=allow[ly])return 0;
  int cx=lx>>3,cy=ly>>3;if(s.hf)cx=s.hsz-1-cx;if(s.vf)cy=s.vsz-1-cy;
  unsigned px=lx&7,py=ly&7;if(s.hf)px=7-px;if(s.vf)py=7-py;
  unsigned a=((s.tile+cx*s.vsz+cy)&2047)*32+py*4+px/2;
  return (v->vram[a]>>((px&1)?0:4))&15;
 }
 // Without shadow/highlight, the RDP's back-to-front order already gives the
 // first SAT sprite the pixel. Two cases still need per-pixel work: a sprite cut
 // by the line budget or masking, and a high-priority sprite that lies behind an
 // earlier low-priority one (the earlier one must still own those pixels).
 // Everything else draws straight from the game's patterns.
 void build_direct(GVDP*v,const X2SpriteDesc*raw,unsigned n,uint8_t(*allowed)[32]){
  count=0;packed=0;
  for(unsigned i=0;i<n;i++){
   const auto&s=raw[i];int w=s.hsz*8,h=s.vsz*8;
   if(s.x+w<=0||s.x>=320||s.y+h<=0||s.y>=224)continue;
   bool cut=false;
   int r0=s.y<0?-s.y:0,r1=s.y+h>224?224-s.y:h;
   for(int r=r0;r<r1;r++)if(allowed[i][r]<w){cut=true;break;}
   uint8_t under[80];unsigned nu=0;
   if(s.pri)for(unsigned j=0;j<i;j++){
    const auto&o=raw[j];
    if(o.pri||o.x>=s.x+w||o.x+o.hsz*8<=s.x||o.y>=s.y+h||o.y+o.vsz*8<=s.y)continue;
    under[nu++]=j;
   }
   if(!cut&&!nu){
    x2_sprite_emit_tiles(s,[&](const X2Tile&t){assert(count<1280);cells[count]=t;direct[count]=1;count++;});
    continue;
   }
   x2_sprite_emit_tiles(s,[&](const X2Tile&t){
    assert(count<1280&&packed<2048);uint8_t*dst=pixels+packed*32;memset(dst,0,32);bool any=false;
    for(unsigned dy=0;dy<t.h;dy++)for(unsigned dx=0;dx<t.w;dx++){
     int x=t.x+dx,y=t.y+dy;
     unsigned nib=pixel_at(v,s,allowed[i],x,y);if(!nib)continue;
     bool owned=false;
     for(unsigned k=0;k<nu&&!owned;k++)owned=pixel_at(v,raw[under[k]],allowed[under[k]],x,y)!=0;
     if(owned)continue;
     dst[dy*4+dx/2]|=nib<<((dx&1)?0:4);any=true;
    }
    if(any){cells[count]={t.x,t.y,t.w,t.h,0,0,uint16_t((t.attr&0xe000)|packed)};direct[count]=0;count++;packed++;}
   });
  }
 }
};
