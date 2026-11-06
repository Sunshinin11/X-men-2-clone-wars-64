#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export N64_INST="${N64_INST:-/opt/libdragon}"
python3 -c 'import sys; sys.path.insert(0,"tools"); import common; common.ensure_rom()'
make -f platform/n64/tests/Makefile native-audio-assets
python3 tools/bridge.py --mode stepped --out build/n64/handbridge
python3 tools/bridge.py --mode native --out build/n64/bridge
make -f platform/n64/game.mk -j"${N64_JOBS:-8}"
