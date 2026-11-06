CXX := g++
CPPFLAGS := -Isegagenesisrecomp/runner -Isegagenesisrecomp/runner/include -Isegagenesisrecomp/runner/external/ymfm/src
AUDIO_OBJECTS := build/n64/hostcheck/sound_core.o build/n64/hostcheck/sound_ym.o build/n64/hostcheck/ymfm_ssg.o build/n64/hostcheck/ymfm_adpcm.o build/n64/hostcheck/ymfm_pcm.o build/n64/hostcheck/sound_z80.o build/n64/hostcheck/sound_psg.o
CPPFLAGS += -DX2_AUDIO_MODE=2 -DX2_SCENE_TEST=1 -DXMEN2_NATIVE=1 -DXMEN2_N64=1 -Iplatform/n64/tests -Igame/include -Igame/runtime -Iplatform -Iplatform/native -Iplatform/n64 -Isegagenesisrecomp/runner/video
FLAGS := -O1 -std=c++17 -fno-delete-null-pointer-checks -fno-strict-aliasing
SOURCES := $(wildcard build/n64/handbridge/handc/*.cpp)
OBJS := $(patsubst build/n64/handbridge/handc/%.cpp,build/n64/hostcheck/o/%.o,$(SOURCES))
all: build/n64/hostcheck/game

# Host-only instrument preparation. None of AUDIO_OBJECTS enters game.mk.
.PHONY: native-audio-assets
native-audio-assets: build/n64/native_audio/voices.pcm build/n64/native_audio/voices.idx
build/n64/hostcheck/bake-audio: tools/n64_bake_audio.cpp $(AUDIO_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) -O2 -std=c++17 -Isegagenesisrecomp/runner -Isegagenesisrecomp/runner/include $< $(AUDIO_OBJECTS) -o $@
build/n64/native_audio/voices.pcm build/n64/native_audio/voices.idx &: build/n64/hostcheck/bake-audio game/xmen2.bin tools/n64_audio_assets.py
	python3 tools/n64_audio_assets.py
	build/n64/hostcheck/bake-audio
build/n64/hostcheck/o/%.o: build/n64/handbridge/handc/%.cpp game/c/%.c
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

.PHONY: native-game
native-game: build/n64/hostcheck/native-game
build/n64/hostcheck/rdp-device.o: platform/n64/rdp_scene.cpp platform/n64/operator_masks.hpp platform/n64/native_sprites.hpp platform/n64/tests/rdp_device/libdragon.h
	$(CXX) -O2 -std=c++17 -Iplatform/n64/tests/rdp_device -Isegagenesisrecomp/runner/video -c $< -o $@
build/n64/hostcheck/effects-check.o: platform/n64/tests/native_effects_check.cpp platform/n64/tests/rdp_device/libdragon.h
	$(CXX) -O2 -std=c++17 -Iplatform/n64/tests/rdp_device -Isegagenesisrecomp/runner/video -c $< -o $@
build/n64/hostcheck/native-game: $(OBJS) build/n64/hostcheck/scene-vdp.o build/n64/hostcheck/rdp-device.o build/n64/hostcheck/effects-check.o platform/n64/machine.cpp platform/n64/sound_native.cpp platform/n64/video_native.c platform/n64/video_native.h platform/n64/audio_n64.hpp platform/n64/native_sequence.hpp
	$(CC) -O2 -Isegagenesisrecomp/runner/video -c platform/n64/video_native.c -o build/n64/hostcheck/video-native.o
	$(CXX) $(filter-out -DX2_AUDIO_MODE=2,$(CPPFLAGS)) -DX2_TEST_RDP_DEVICE=1 -DX2_AUDIO_MODE=3 -DX2_NATIVE_SAMPLE_AUDIO=1 -include platform/n64/video_native.h $(FLAGS) -O0 platform/n64/tests/scene_check.cpp platform/n64/machine.cpp platform/n64/memory.cpp game/runtime/players34.cpp build/n64/bridge/xmen2_bridge.cpp platform/n64/sound_native.cpp $(OBJS) build/n64/hostcheck/scene-vdp.o build/n64/hostcheck/video-native.o build/n64/hostcheck/rdp-device.o build/n64/hostcheck/effects-check.o -o $@
build/n64/hostcheck/vdp.o: platform/n64/vdp_bridge.c
	gcc -O1 -DGENESIS_BATCHED_PLANES=1 -c $< -o $@
build/n64/hostcheck/game: platform/n64/audio_n64.hpp platform/n64/audio_commands.hpp platform/n64/sound_core.hpp $(AUDIO_OBJECTS) $(OBJS) build/n64/hostcheck/vdp.o platform/n64/machine.cpp platform/n64/memory.cpp game/runtime/players34.cpp build/n64/bridge/xmen2_bridge.cpp platform/n64/tests/libdragon.h platform/n64/scene.hpp platform/n64/page_commands.hpp platform/n64/tests/scene_check.cpp
	$(CXX) $(CPPFLAGS) $(FLAGS) platform/n64/tests/scene_check.cpp platform/n64/machine.cpp platform/n64/memory.cpp game/runtime/players34.cpp build/n64/bridge/xmen2_bridge.cpp $(OBJS) $(AUDIO_OBJECTS) build/n64/hostcheck/vdp.o -o $@

.PHONY: scene-check
scene-check: build/n64/hostcheck/scene-test
	./build/n64/hostcheck/scene-test
build/n64/hostcheck/scene-vdp.o: platform/n64/vdp_bridge.c segagenesisrecomp/runner/video/genesis_vdp.c segagenesisrecomp/runner/video/genesis_vdp.h
	@mkdir -p $(@D)
	gcc -O2 -DGENESIS_BATCHED_PLANES=1 -c $< -o $@
build/n64/hostcheck/scene-test: platform/n64/tests/test_scene.cpp platform/n64/tests/scene_check.cpp platform/n64/scene.hpp platform/n64/page_commands.hpp build/n64/hostcheck/scene-vdp.o
	$(CXX) -O2 -std=c++17 -Iplatform/n64 -Isegagenesisrecomp/runner/video platform/n64/tests/test_scene.cpp platform/n64/tests/scene_check.cpp build/n64/hostcheck/scene-vdp.o -o $@

build/n64/hostcheck/sound_core.o: platform/n64/sound_core.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/sound_ym.o: platform/n64/sound_ym.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/ymfm_opn.o: segagenesisrecomp/runner/external/ymfm/src/ymfm_opn.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/ymfm_ssg.o: segagenesisrecomp/runner/external/ymfm/src/ymfm_ssg.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/ymfm_adpcm.o: segagenesisrecomp/runner/external/ymfm/src/ymfm_adpcm.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/ymfm_pcm.o: segagenesisrecomp/runner/external/ymfm/src/ymfm_pcm.cpp platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -c $< -o $@

build/n64/hostcheck/sound_z80.o: segagenesisrecomp/runner/external/superzazu/z80.c platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	gcc $(CPPFLAGS) -O2 -c $< -o $@

build/n64/hostcheck/sound_psg.o: platform/n64/sound_psg.c platform/n64/sound_core.hpp
	@mkdir -p $(@D)
	gcc $(CPPFLAGS) -O2 -c $< -o $@


.PHONY: synth-rate-check
SYNTH_OBJECTS := $(filter %/sound_ym.o %/sound_psg.o %/ymfm_ssg.o %/ymfm_adpcm.o %/ymfm_pcm.o,$(AUDIO_OBJECTS))
synth-rate-check: build/n64/hostcheck/synth-rate-test
	./build/n64/hostcheck/synth-rate-test
build/n64/hostcheck/synth-rate-test: platform/n64/tests/test_synth_rate.cpp $(SYNTH_OBJECTS)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 $< $(SYNTH_OBJECTS) -o $@


$(OBJS): $(wildcard game/include/*.h) platform/n64/tests/Makefile
-include $(OBJS:.o=.d)
