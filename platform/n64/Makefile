# First cross-compilation gate for the native game logic. Not a playable ROM.
N64_INST ?= /opt/libdragon
CXX := $(N64_INST)/bin/mips64-elf-g++
SOURCES := $(wildcard game/c/*.c)
OBJECTS := $(patsubst game/c/%.c,build/n64/handc_be/%.o,$(SOURCES))
CPPFLAGS := -DXMEN2_NATIVE=1 -DXMEN2_N64=1 -Igame/include -Igame/runtime -Iplatform -Iplatform/native
CXXFLAGS := -std=gnu++17 -O2 -march=vr4300 -mtune=vr4300 -mabi=32 -G0 -fno-strict-aliasing -fno-delete-null-pointer-checks
.PHONY: handc
handc: $(OBJECTS)
build/n64/handc_be/%.o: game/c/%.c
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -c $< -o $@

build/n64/bridge/xmen2_bridge.o: build/n64/bridge/xmen2_bridge.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
