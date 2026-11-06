# Current native target

See [NATIVE_MIGRATION.md](NATIVE_MIGRATION.md) for the current architecture, build, validation and audio limitations. The historical notes below describe earlier implementations and are superseded by that document.

# N64 port ? initial cross-compilation

The N64 target starts with the native hand-written C, not the 68000 runtime.
`make -f platform/n64/Makefile handc` cross-compiles the game's translation
units into `build/n64/handc/`. This target does not link a game or generate ROM.

The supplied image has SHA-1 1ee0078bc57f39e5f78e5c24660c70dc62f7ba47.
It is not the reference image, but its reset vector (0x1f43ae) and VBlank
vector (0x1f4860) match the function catalog. This is only a structural check;
asset/code compatibility has not been established. The PC hash guard remains
unchanged. Do not publish or commit either ROM image.

Next boundaries to implement:
- Replace the PC double-mapped RAM arena and little-endian tag assumptions
  with N64-safe RAM access, testing both stored pointer forms and mixed widths.
- Generate the native function bridge using the existing generator.
- Connect VDP output and controller polling to libdragon.
- Bring up audio after the first playable frame.

No N64 gameplay or performance claim is made by a successful object build.

The image was subsequently replaced by the reference USA/Europe image:
SHA-1 61409e6cf6065ab67d8952b891d8edcf47777193. The input now uses the
standard game/xmen2.bin name; the translated image remains separate.

Initial cross-compilation result: all 3164 hand-C translation units and the
generated native bridge compile for VR4300/o32. No compiler errors. One
existing warning: Rand_Seed(99999) narrows to signed short in 03A2A6.c;
the resulting wrap must be preserved until checked against original behavior.
This validates compilation only, not linkage, memory safety, rendering or play.

Memory adaptation implemented in memory.cpp and XMEN2_N64 branches of
xmen2_mem.h. N64 builds bypass little-endian tag fast paths and store canonical
big-endian RAM bytes. Both distinct pointer windows normalize to the same
64 KiB backing region through scalar accessors. Host test_memory.cpp passed:
32-bit stores, signed-width-independent byte reads, unaligned 16-bit overlap,
both pointer forms, distinct guest address round trips, and big-endian ROM.
Raw host pointer accesses outside the scalar wrappers still require audit.
This is not yet linked into the diagnostic ROM; no game execution is claimed.

## Native game integration

`bash build_n64.sh` now targets platform/n64/game.mk, not the diagnostic
boot target. It generates the official stepped wrappers and native bridge,
compiles the hand C with XMEN2_N64 and links the memory, VDP and controller
platform. The supplied ROM is validated by the existing reference hash guard.

The game platform calls EntryPoint and the real VBlank vector. It renders
through the existing clean-room VDP into a 320x240 RGBA16 framebuffer using
its DAC palette. N64 A/B/C-right map to Genesis A/B/C; D-pad and Start are
connected. Audio ports currently retain mailbox storage and bus-grant behavior
but no Z80/chip playback is integrated: this initial gameplay target is silent.
PC enhancements and netplay are disabled by zero-valued options.

A separate headless host sink exercised this same platform and native game:
120 frames completed and the captured framebuffer showed the initial snow
stage, HUD and character. This is host integration evidence, not an N64
hardware or emulator performance result.

The initial full N64 link exceeded RDRAM. The corrected build uses canonical
big-endian memory, the reference image's actual 2 MiB ROM extent, and -Os with
out-of-line scalar accessors to avoid repeated template expansion. The linker
memory checks remain enabled.

Validation: 600 headless frames completed with right and A input. The
captured scene scrolled through the initial snow stage with its character
and HUD. The IRQ path also restores the stored-call d0 around VBlank, as the
original platform does. Host tests live in platform/n64/tests (a fake display
and controller sink, not an emulator and not evidence of N64 FPS).

Initial successful game link: text 2440040, data 813580, BSS 2772313,
total 6025933 bytes. This target requires the 8 MiB Expansion Pak. The
linker's RDRAM bounds check is active. Real N64/Ares runtime validation and
audio playback remain separate from this host integration test.

Final game build linked successfully: text 1984040, data 780704,
BSS 2772313, total 5537057 bytes. xmen2_n64.z64 is 3702784 bytes.
The native game EntryPoint is now the ROM entry path; the diagnostic main
is not linked by game.mk. Final host test completed 600 frames with input
after the IRQ-state correction. bash syntax check passed. No link errors.
Audio playback and real N64/Ares execution are not verified by these tests.

## First performance change after emulator bring-up

User observed correct snow-stage rendering but slow gameplay. VPS is not the
game's frame rate. CPU framebuffer stores now use the cached alias followed
by explicit writeback of the complete 320x240 RGBA16 image before display_show.
All pixels are overwritten; no RDP drawing or DMA writes share these buffers.
This reduces uncached per-pixel RAM traffic without changing game stepping.
X2_PERF reports every 60 presentations: fps100, logic_us (between presents,
including VBlank work), wait_us (display_get), render_us (VDP plus conversion
and framebuffer writeback), present_us (show and controller poll). First
window excludes pre-first-present startup. Timers use unsigned clock deltas.
No performance gain is asserted until the new emulator log is measured.

Performance build completed: text 1984456, data 780772, BSS 2772361,
total 5537589. No compile/link errors. Cached framebuffer writes plus explicit
writeback are active only in the N64 branch; the original host test sink is
unchanged. Actual speedup awaits emulator telemetry. Single ROM updated.

## Detailed rendering cost measurement

Baseline from four 60-frame X2_PERF windows: weighted 6.4873 FPS;
logic 8.37375 ms, wait 2.7625 ms, render 142.8175 ms, present 0.048 ms.
X2_RENDER now separates vdp_us, pixels_us and writeback_us per frame.
Only timing reads surround scanline production/conversion; no extra image
buffer or rendering algorithm change. Counters aggregate and reset with
X2_PERF every 60 presentations. Compilation/link passed: total 5537921 bytes.
This build measures the next target; no speedup is claimed for instrumentation.

## RDP tile composition

The normal 320x224, non-interlaced, non-shadow/highlight path now composes
planes B/A and sprite layers with the RDP. The original clean-room sprite
scanline evaluator is retained for SAT ordering, clipping, limits and collision
flags. Planes use native CI4 VRAM patterns and per-palette TLUT loads; sprites
use CI8 low/high layers. Layer order is B-low, A-low, S-low, B-high, A-high,
S-high. Transparent pattern color zero remains transparent in each palette.
Vertical coalescing merges only matching tile/UV/priority runs; per-line and
per-column scroll, window boundaries and flips remain explicit. Unsupported
modes use the original complete software renderer.

All source DMA data is written back before submission and remains unchanged
until rdpq_detach_wait completes. This first version waits rather than racing
game VRAM updates against the RDP. Point filtering and no dithering preserve
palette pixels. GPU output is not overwritten by the CPU framebuffer path.

Validation: 80 synthetic scenes matched reference pixels, including scroll,
windows, flipped tiles and sprite priority; sprite collision, overflow and
edge state matched. 600 real snow-stage frames with movement/action matched
reference pixels using a software rasterization of the native tile commands.
This validates command composition, not the physical RDP texture upload/blit.
Ares/N64 image and speed validation remains the next runtime check.

X2_RENDER reports gpu_us and native_frames per 60 presentations. X2_RDP
reports tile rectangle totals and averaged sprite_us, commands_us, and
residual gpu_wait_us per 60 native frames. These are child costs of render_us,
not additional costs to add to it. Audio remains unimplemented.

Reproduce the asset-free scene tests with:
`make -f platform/n64/tests/Makefile scene-check`.
Pixel comparisons cover displayed RGB (the framebuffer alpha/coverage bit is
excluded); transparent tile/sprite colors are checked through their composite.

Final RDP build: text 2026856, data 796880, BSS 2919113,
total 5742849 bytes. Compile/link passed with no new warnings.
Scene-check passed after final tile coalescing changes. Real-stage host
comparison completed 600 frames. Single xmen2_n64.z64 updated; actual RDP
blit appearance and speed await emulator telemetry.

