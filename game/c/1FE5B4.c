/* unk_Snd_1FE5B4 - set master attenuation (GEMS command $20), unless muted.
 *
 * arg: master attenuation (low 8 bits; 0 is full volume).
 * Hand-written 68000 (Snd_Play's tail with command $20). */
#include "xmen2.h"

void unk_Snd_1FE5B4(long arg)
{
	unsigned short sr;
	short pos = Z80_BusAcquire(&sr);

	if (!g_SndMute) {
		pos = Snd_PutCmd(0x20, pos);
		Snd_PutByte((char)arg, pos);
	}
	Z80_BusRelease(sr);
}
