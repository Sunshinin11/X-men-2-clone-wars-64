N64_INST ?= /opt/libdragon
BUILD_DIR := build/n64/game
ROM_NAME := xmen2_heavy_60_opt
N64_ROM_TITLE := "XMen2 Clone Wars"
N64_ROM_SAVETYPE := none
include $(N64_INST)/include/n64.mk
.DEFAULT_GOAL := all
X2_AUDIO_MODE ?= 3
ifneq ($(X2_AUDIO_MODE),3)
$(error Production N64 audio requires native sample mode 3)
endif
CPPFLAGS := -MMD -MP -fno-strict-aliasing -include platform/n64/video_native.h \
	-DXMEN2_NATIVE=1 -DXMEN2_N64=1 \
	-DX2_AUDIO_MODE=$(X2_AUDIO_MODE) \
	-DX2_PRESENT_VBLANKS=1 \
	-DX2_AUDIO_OPT=1 \
	-DX2_PACE_SLICE_OPT=1 \
	-DX2_PLANE_CMD_CACHE=1 \
	-Isegagenesisrecomp/runner -Isegagenesisrecomp/runner/include \
	-Iplatform/n64 -Igame/include -Igame/runtime -Iplatform -Iplatform/native -Isegagenesisrecomp/runner/video
SOURCES := $(wildcard build/n64/handbridge/handc/*.cpp)
OBJECTS := $(patsubst build/n64/handbridge/handc/%.cpp,$(BUILD_DIR)/handc/%.o,$(SOURCES))
EXTRA := $(BUILD_DIR)/machine.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bridge.o $(BUILD_DIR)/vdp.o $(BUILD_DIR)/players34.o $(BUILD_DIR)/rdp_scene.o $(BUILD_DIR)/sound_native.o

.PHONY: all native-link-check
all: $(ROM_NAME).z64

$(BUILD_DIR)/handc/%.o: build/n64/handbridge/handc/%.cpp game/c/%.c game/include/xmen2_mem.h
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -Wno-error -Os -fno-unroll-loops -fno-delete-null-pointer-checks -c $< -o $@

$(BUILD_DIR)/machine.o: platform/n64/machine.cpp platform/n64/audio_n64.hpp platform/n64/audio_commands.hpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -Wno-error -Os -fno-unroll-loops -c $< -o $@

$(BUILD_DIR)/memory.o: platform/n64/memory.cpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -Wno-error -Os -fno-unroll-loops -c $< -o $@

$(BUILD_DIR)/bridge.o: build/n64/bridge/xmen2_bridge.cpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -Wno-error -Os -fno-unroll-loops -fno-delete-null-pointer-checks -c $< -o $@

$(BUILD_DIR)/players34.o: game/runtime/players34.cpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -Wno-error -Os -fno-unroll-loops -c $< -o $@

$(BUILD_DIR)/vdp.o: platform/n64/video_native.c platform/n64/video_native.h
	@mkdir -p $(@D)
	$(N64_CC) $(N64_CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/rdp_scene.o: platform/n64/rdp_scene.cpp platform/n64/scene.hpp platform/n64/page_commands.hpp platform/n64/sprite_scene.hpp platform/n64/operator_masks.hpp platform/n64/native_sprites.hpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -c $< -o $@

$(BUILD_DIR)/sound_native.o: platform/n64/sound_native.cpp platform/n64/sound_core.hpp platform/n64/native_sequence.hpp platform/n64/pitch_table.hpp
	@mkdir -p $(@D)
	$(N64_CXX) $(N64_CXXFLAGS) $(CPPFLAGS) -std=gnu++17 -O2 -c $< -o $@

$(BUILD_DIR)/$(ROM_NAME).elf: $(OBJECTS) $(EXTRA)
$(BUILD_DIR)/$(ROM_NAME).elf: CXX=$(N64_CXX)
$(BUILD_DIR)/$(ROM_NAME).elf: LDFLAGS=$(N64_LDFLAGS)

# Rate variants of the baked voices (tools/n64_trim_audio.cpp); the N64 plays these.
build/n64/native_audio/voices_v.pcm build/n64/native_audio/voices_v.idx &: tools/n64_trim_audio.cpp build/n64/native_audio/voices.pcm build/n64/native_audio/voices.idx
	g++ -O2 -std=c++17 tools/n64_trim_audio.cpp -o build/n64/hostcheck/trim-audio
	build/n64/hostcheck/trim-audio build/n64/native_audio

$(BUILD_DIR)/$(ROM_NAME).dfs: game/xmen2.bin build/n64/native_audio/voices_v.pcm build/n64/native_audio/voices_v.idx
	@mkdir -p $(BUILD_DIR)/fs
	cp $< $(BUILD_DIR)/fs/xmen2.bin
	cp build/n64/native_audio/voices_v.pcm $(BUILD_DIR)/fs/voices.pcm
	cp build/n64/native_audio/voices_v.idx $(BUILD_DIR)/fs/voices.idx
	$(N64_MKDFS) $@ $(BUILD_DIR)/fs

$(ROM_NAME).z64: $(BUILD_DIR)/$(ROM_NAME).dfs
$(ROM_NAME).z64: $(BUILD_DIR)/$(ROM_NAME).elf
$(ROM_NAME).z64: native-link-check

native-link-check: $(BUILD_DIR)/$(ROM_NAME).elf
	python3 tools/n64_native_audit.py $<

$(EXTRA): game/include/xmen2_mem.h platform/n64/video_native.h

-include $(OBJECTS:.o=.d) $(EXTRA:.o=.d)

$(OBJECTS) $(EXTRA): platform/n64/game.mk
