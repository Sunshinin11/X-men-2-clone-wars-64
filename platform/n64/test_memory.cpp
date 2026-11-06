#include <cassert>
#include "xmen2.h"
extern "C" void xm_native_mem_init();
extern "C" { uint8_t xm_region[0x10000]; }
int main(){xm_native_mem_init();
 auto*a=(u32*)xm::host(0xff0100);auto*b=(u32*)xm::host(0xffff0100);
 *a=0x12345678;assert((uint32_t)*b==0x12345678);
 assert((uint32_t)*(u8*)xm::host(0xff0101)==0x34);
 *(u16*)xm::host(0xffff0101)=0xabcd;assert((uint32_t)*a==0x12abcd78);
 assert(xm::addr(a)==0xff0100&&xm::addr(b)==0xffff0100);
 g_rom[4]=0x12;g_rom[5]=0x34;assert((uint16_t)*(u16*)(g_rom+4)==0x1234);
}
