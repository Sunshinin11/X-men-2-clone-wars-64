#include <libdragon.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static unsigned char header[512];
int main(void){
 debug_init_isviewer();debug_init_usblog();
 console_init();console_set_render_mode(RENDER_MANUAL);
 dfs_init(DFS_DEFAULT_LOCATION);joypad_init();
 int fd=dfs_open("xmen2.bin");int n=fd<0?-1:dfs_read(header,1,sizeof(header),fd);
 if(fd>=0)dfs_close(fd);
 unsigned reset=n==512?((unsigned)header[4]<<24|(unsigned)header[5]<<16|(unsigned)header[6]<<8|header[7]):0;
 unsigned irq=n==512?((unsigned)header[120]<<24|(unsigned)header[121]<<16|(unsigned)header[122]<<8|header[123]):0;
 bool valid=n==512&&!memcmp(header+256,"SEGA",4)&&reset==0x1f43ae&&irq==0x1f4860;
 debugf("[X2_N64] platform_boot=1 game_running=0 rom_header=%d reset=%08x vint=%08x\n",valid,reset,irq);
 for(;;){
  joypad_poll();joypad_buttons_t b=joypad_get_buttons_held(JOYPAD_PORT_1);
  console_clear();printf("X-MEN 2 - N64 PLATFORM\n\n");
  printf("Diagnostic build - gameplay not connected\n\n");
  printf("ROM header: %s\n",valid?"OK":"FAILED");
  printf("Reset: %08lx\nVBlank: %08lx\n\n",(unsigned long)reset,(unsigned long)irq);
  printf("Controller: A=%d B=%d Start=%d\n",b.a,b.b,b.start);
  printf("D-pad: %d %d %d %d\n",b.d_up,b.d_down,b.d_left,b.d_right);
  printf("\nNative C cross-compilation: 3164 functions\n");
  printf("Next: big-endian memory and VDP integration\n");
  console_render();
 }
}
