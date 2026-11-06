"""Reject runtime emulation in the production N64 executable before ROM packing."""
import os
import re
import subprocess
import sys
import struct
from pathlib import Path

elf = Path(sys.argv[1])
nm = Path(os.environ.get("N64_INST", "/opt/libdragon")) / "bin/mips64-elf-nm"
symbols = subprocess.check_output([str(nm), "-C", "--defined-only", str(elf)], text=True)
forbidden = re.compile(r"\b(?:z80_(?:exec_one|step|init|take_irq|base_cycles)|"
                       r"ym2612_\w+|psg_(?:init|write|advance|render)|ymfm::|"
                       r"gvdp_(?:render_scanline|begin_scanline|write_control|write_data|rb_pending_stall)|"
                       r"x2_sprite_line)\b")
found = [line.strip() for line in symbols.splitlines() if forbidden.search(line)]
if found:
    sys.exit("Runtime emulation in N64 link:\n" + "\n".join(found))
for required in ("x2_video_command", "x2_video_upload", "x2_rdp_draw", "x2_sound_pump()"):
    if required not in symbols:
        sys.exit("Missing native subsystem: " + required)
assets = elf.parent.parent / "native_audio"
# Validate the exact rate-variant files packed by game.mk, not the old source bank.
index_path = assets / "voices_v.idx"
pcm_path = assets / "voices_v.pcm"
index = index_path.read_bytes()
if len(index) != 8 + 352 * 4 * 16 or index[:8] != struct.pack(">II", 0x58325633, 352):
    sys.exit("Invalid production variant index")
extent = pcm_path.stat().st_size
for slot in range(352):
    for variant in range(4):
        offset,count,rate,kind=struct.unpack_from(">IIII",index,8+(slot*4+variant)*16)
        if offset & 1 or offset + count*2 > extent or not 0 < rate <= 65536 or (kind & 255)>3:
            sys.exit(f"Invalid production asset {slot}/{variant}")
        if kind & 256 and not 0 < kind >> 16 <= count:
            sys.exit(f"Invalid production loop {slot}/{variant}")
print("N64 LINK AUDIT PASS: native video/RDP/RSP sampled audio; all 1408 packed variants validated")
