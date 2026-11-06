"""Reject silence and missing original-driver activity; local audio stays in build/."""
from pathlib import Path
import array, math, re, sys, wave
raw=Path("build/n64/hostcheck/original-audio.pcm").read_bytes()
a=array.array("h");a.frombytes(raw)
if sys.byteorder!="little":a.byteswap()
assert len(a)>12000*2*5, "less than five seconds of captured original sound"
assert sum(x!=0 for x in a)>len(a)//10, "silence or near-silence"
peak=max(map(abs,a));assert 100<peak<32767, "inaudible or clipped output"
log=Path(sys.argv[1] if len(sys.argv)>1 else "build/n64/original-audio-host.log").read_text()
rows=re.findall(r"\[X2_PCM_HOST\].*?keyons=(\d+) fm=(\d+) psg=(\d+)",log)
assert rows and int(rows[-1][0])>0 and int(rows[-1][1])>100 and int(rows[-1][2])>0, "original FM/PSG driver inactive"
assert "SCENE PASS frames=600" in log, "real scene comparison incomplete"
with wave.open("build/n64/hostcheck/original-audio.wav","wb") as w:
 w.setnchannels(2);w.setsampwidth(2);w.setframerate(12000);w.writeframes(raw)
print(f"ORIGINAL AUDIO PASS stereo_samples={len(a)//2} nonzero={sum(x!=0 for x in a)} peak={peak} rms={math.sqrt(sum(x*x for x in a)/len(a)):.2f}; physical output pending user validation")
