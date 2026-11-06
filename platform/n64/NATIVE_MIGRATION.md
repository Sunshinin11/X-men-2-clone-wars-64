# N64 native migration — 2026-11-03

## Production architecture

The N64 target compiles the existing C/C++ gameplay to MIPS. Original addresses remain identifiers for game state, data and function dispatch; there is no 68000 instruction interpreter.

Video storage and asset uploads now use video_native.c. The Genesis VDP implementation is not linked into the production executable. Tile, palette, sprite and scroll data retain the game's packed formats, adapted by lightweight commands. There is no Genesis raster clock, FIFO simulation, DMA cycle charging or software scanline renderer. Frames, including display-off transitions, are submitted through the N64 RDP.

Shadow/highlight in SpaceA1 uses native palette material passes. Operator sprites select shadow or highlight material through two-texture RDP masks, with opaque-sprite ownership and combined operators. Colors are taken from the original normal/shadow/highlight palettes rather than approximated with black/white blending. The native sprite-packet builder prepares clipped CI4 textures and alpha masks, preserving overlap order and the selection screen clipping rules; it does not calculate an RGB framebuffer.

Audio uses native_sequence.hpp and sound_native.cpp. GEMS musical data is sequenced directly in C++, including nested songs, character parameters, conditions, loops, effects/music clocks, patch changes, priority, pitch bend, envelopes and pause/resume/stop. Eight native sample voices stream small PCM blocks from the cartridge filesystem. The AI callback consumes a bounded ring; filesystem reads and sequencing happen outside the interrupt.

The Z80 executable is no longer uploaded by Z80_LoadDriver on N64. The Z80 interpreter, YM2612/PSG implementations and ymfm library are excluded from game.mk. Old sound-machine sources are retained exclusively for host-side asset preparation and reference tests.

## Offline assets

n64_audio_assets.py validates the source cartridge SHA-1 and exports its four data banks, without the executable driver. It validates all 256 song headers and 577 track pointers.

n64_bake_audio.cpp runs on the computer, using the original audio reference to prepare the instrument recordings. It produces 352 indexed assets: the patch instruments and all 96 DAC sample-note slots. Silent tails are trimmed and sustain-loop seams are selected offline. These recordings are packaged as voices.idx and voices.pcm; the sound machine that created them is not packaged in the N64 executable.

The audio is a sample-based adaptation, not bit-exact chip synthesis: tonal patches are recorded at C4 and transposed, embedded modulation changes speed with transposition, output is stereo at approximately 12 kHz, release handling is native and voice allocation uses a 8-voice sample pool. These differences must not be described as perfect original-chip fidelity.

## Build

Run bash build_n64.sh in the configured Linux/libdragon environment. Host-only asset preparation runs before the N64 build. The native link audit is a required prerequisite of ROM packing, including direct game.mk builds. It rejects Z80 interpreter, FM/PSG synthesis and Genesis software-renderer symbols and requires the native subsystems.

The target still requires the 8 MiB Expansion Pak. The complete original cartridge data remains a resource; its CPU instructions are not executed. No claim of real-console FPS follows from compilation or host tests.

## Validation

- Native MIPS modules compile and the executable links with RDRAM bounds checking.
- Defined-symbol audit confirms no Z80/chip/software-VDP runtime in the N64 executable.
- Native video upload/read/copy/fill/transfer data matches the reference in 2000 generated cases.
- Native musical sequencing: 256 songs x 8 character settings x 3600 ticks, 477998 note events, zero parser failures under address/undefined-behavior sanitizers.
- Native sampled audio tests verify nonzero output, deterministic replay, different character music parts, nested songs, parameter readback and stop-all silence.
- AI/ring test verifies continuous nonzero PCM delivery with a 20 FPS producer and variable callback lengths, without buffer overruns or underruns in that test.
- Production RDP texture-command tests verify normal, shadow and highlight colors, priorities and operator masks against the reference.
- Final integrated checks passed 180 frames in each of six scenes: Siberia, player selection, Cerebro Space, SpaceB1, SpaceA1 and CloneA1. The production RDP command stream matched the reference RGB frames, including the corrected selection-screen clipping and SpaceA1 shadow/highlight.

Host comparisons exercise a test implementation of RDP commands; they do not establish physical RDP behavior or hardware performance. Any subsequent emulator run is a test of the N64 ROM, not an emulator embedded in the port.


## Delivered artifact

ROM: xmen2_n64.z64, 24,117,248 bytes.
SHA-256: a1468bf127c6c53b89979be3fa92ba8db6ab62a404796afb3379d1656b655e88.
Final link: text 2,065,032; data 815,660; BSS 2,815,665; total 5,696,357 bytes (runtime framebuffer/heap allocations are additional).
The standard libdragon ROM build completed successfully; N64 header and cartridge-size checks passed. Ares was not started. Console/emulator gameplay testing is reserved for the user.

Reproduce internal checks with bash platform/n64/tests/run_native.sh --integration.



