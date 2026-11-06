#pragma once
#include "sprite_scene.hpp"
#include <string.h>

// Alpha-only material selections prepared with native sprite packets.
// 0 = highlight, 1 = shadow, 2 = both operators (normal material).
struct X2OperatorMasks {
 alignas(16) uint8_t mask[3][320*224];
 uint8_t occupied[320*224/8];
 uint8_t tiles[3][40*28];
 bool any(unsigned which)const{for(unsigned i=0;i<40*28;i++)if(tiles[which][i])return true;return false;}
};
