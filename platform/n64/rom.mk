N64_INST ?= /opt/libdragon
BUILD_DIR := build/n64/boot
N64_ROM_TITLE := "XMen2 N64 Platform"
N64_ROM_SAVETYPE := none
include $(N64_INST)/include/n64.mk
.DEFAULT_GOAL := all
.PHONY: all
all: xmen2_n64.z64
$(BUILD_DIR)/main.o: platform/n64/main.c
	@mkdir -p $(@D)
	$(N64_CC) $(N64_CFLAGS) -c $< -o $@
$(BUILD_DIR)/xmen2_n64.elf: $(BUILD_DIR)/main.o
$(BUILD_DIR)/xmen2_n64.dfs: game/xmen2.bin
	@mkdir -p $(BUILD_DIR)/fs
	cp game/xmen2.bin $(BUILD_DIR)/fs/xmen2.bin
	$(N64_MKDFS) $@ $(BUILD_DIR)/fs
xmen2_n64.z64: $(BUILD_DIR)/xmen2_n64.dfs
xmen2_n64.z64: $(BUILD_DIR)/xmen2_n64.elf
-include $(wildcard $(BUILD_DIR)/*.d)
