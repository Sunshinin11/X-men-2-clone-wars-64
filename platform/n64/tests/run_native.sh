#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
mkdir -p build/n64/hostcheck
python3 platform/n64/tests/check_pitch_table.py
make -f platform/n64/tests/Makefile native-audio-assets scene-check
flags=(-std=c++17 -O2 -fsanitize=address,undefined)
g++ "${flags[@]}" platform/n64/tests/test_audio.cpp -o build/n64/hostcheck/audio-command-test
build/n64/hostcheck/audio-command-test
g++ "${flags[@]}" platform/n64/tests/test_native_sequence.cpp -o build/n64/hostcheck/native-sequence-test
build/n64/hostcheck/native-sequence-test
g++ "${flags[@]}" platform/n64/tests/test_native_audio.cpp platform/n64/sound_native.cpp -o build/n64/hostcheck/native-audio-test
build/n64/hostcheck/native-audio-test
g++ "${flags[@]}" -Iplatform/n64/tests/audio_device platform/n64/tests/test_audio_device.cpp -o build/n64/hostcheck/audio-device-test
build/n64/hostcheck/audio-device-test
gcc -O2 -Isegagenesisrecomp/runner/video -c platform/n64/video_native.c -o build/n64/hostcheck/video-native-checked.o
g++ -O2 -std=c++17 -Isegagenesisrecomp/runner/video platform/n64/tests/test_native_video.cpp build/n64/hostcheck/video-native-checked.o build/n64/hostcheck/scene-vdp.o -o build/n64/hostcheck/native-video-test
build/n64/hostcheck/native-video-test
g++ "${flags[@]}" -Isegagenesisrecomp/runner/video platform/n64/tests/test_operator_masks.cpp build/n64/hostcheck/scene-vdp.o -o build/n64/hostcheck/operator-mask-test
build/n64/hostcheck/operator-mask-test
g++ -O2 -std=c++17 -Iplatform/n64/tests/rdp_device -Isegagenesisrecomp/runner/video platform/n64/rdp_scene.cpp platform/n64/tests/test_rdp_effects.cpp build/n64/hostcheck/scene-vdp.o -o build/n64/hostcheck/rdp-effects-test
build/n64/hostcheck/rdp-effects-test
if [[ "${1:-}" == --integration ]]; then
 make -f platform/n64/tests/Makefile -j8 native-game
 for scene in 0 3 34 35 37 39; do
  X2_TEST_SCENE=$scene X2_HOST_FRAMES=180 build/n64/hostcheck/native-game > "build/n64/native-scene-$scene.log" 2>&1
 done
fi
g++ "${flags[@]}" -DXMEN2_NATIVE=1 -DXMEN2_N64=1 -Igame/include -Igame/runtime -Iplatform -Iplatform/native platform/n64/test_memory.cpp platform/n64/memory.cpp -o build/n64/hostcheck/native-memory-test
build/n64/hostcheck/native-memory-test
g++ "${flags[@]}" platform/n64/tests/test_native_pacing.cpp -o build/n64/hostcheck/native-pacing-test
build/n64/hostcheck/native-pacing-test
echo 'NATIVE CHECKS PASS' 
