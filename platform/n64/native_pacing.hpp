#pragma once
#include <stdint.h>
static inline bool x2_vblank_reached(uint32_t now,uint32_t target){return int32_t(now-target)>=0;}
static inline uint32_t x2_next_present_vblank(uint32_t now,uint32_t last,bool has_last,bool&late,uint32_t interval=2){
 uint32_t target=has_last?last+interval:now+1;
 late=has_last&&x2_vblank_reached(now,target);
 return late?now+1:target;
}
