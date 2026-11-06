#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
make -f platform/n64/tests/Makefile scene-check synth-rate-check
c++ -std=c++17 -O1 -fsanitize=address,undefined -Iplatform/n64/tests/audio_device platform/n64/tests/test_audio_device.cpp -o build/n64/hostcheck/audio-device-test
build/n64/hostcheck/audio-device-test
c++ -std=c++17 -fsanitize=address,undefined platform/n64/tests/test_audio.cpp -o build/n64/hostcheck/audio-test
build/n64/hostcheck/audio-test
make -f platform/n64/tests/Makefile -j8
build/n64/hostcheck/game > build/n64/snow-real-frames.log 2>&1
python3 platform/n64/tests/check_original_pcm.py build/n64/snow-real-frames.log
c++ -O2 -std=c++17 -Iplatform/n64 -Isegagenesisrecomp/runner/video platform/n64/tests/replay_scene.cpp platform/n64/tests/scene_check.cpp build/n64/hostcheck/scene-vdp.o -o build/n64/hostcheck/replay-scene
states=(build/n64/hostcheck/snow-{120,300,600}.state)
X2_NATIVE_SPRITES=1 build/n64/hostcheck/replay-scene "${states[@]}"
# Negative control: this must fail on the same real state.
if X2_REPRO_DISCARD=1 build/n64/hostcheck/replay-scene "${states[0]}"; then
 echo 'ERROR: real-state test missed the original snow regression' >&2
 exit 1
fi
