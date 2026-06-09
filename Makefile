CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
PKG_CONFIG ?= pkg-config

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4)

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TARGET := $(BIN_DIR)/aquarium

VENDOR_DIR := vendor
VENDOR_SRC := $(VENDOR_DIR)/tomlc99/toml.c
VENDOR_OBJ := $(OBJ_DIR)/toml.o

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRC)) $(VENDOR_OBJ)

FISH_DIR := Fish
ASSET_DIR := assets

.PHONY: all clean assets bear run install-conf

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(GTK_LIBS) -lm

$(OBJ_DIR)/%.o: src/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(VENDOR_DIR)/tomlc99 $(GTK_CFLAGS) -c $< -o $@

$(VENDOR_OBJ): $(VENDOR_SRC)
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(VENDOR_DIR)/tomlc99 $(GTK_CFLAGS) -c $< -o $@

assets:
	mkdir -p $(ASSET_DIR)
	ffmpeg -i $(FISH_DIR)/shark.webp -vf "scale=100:-1" -frames:v 1 -y $(ASSET_DIR)/shark.png
	ffmpeg -i $(FISH_DIR)/bass.webp  -vf "scale=80:-1"  -frames:v 1 -y $(ASSET_DIR)/bass.png
	ffmpeg -i $(FISH_DIR)/trout.webp -vf "scale=80:-1"  -frames:v 1 -y $(ASSET_DIR)/trout.png
	ffmpeg -i $(FISH_DIR)/tuna.webp  -vf "scale=80:-1"  -frames:v 1 -y $(ASSET_DIR)/tuna.png
	ffmpeg -i $(FISH_DIR)/fish1Texture.png -vf "hflip" -frames:v 1 -y $(ASSET_DIR)/goldfish.png
	ffmpeg -i $(FISH_DIR)/fish2Texture.png -vf "hflip" -frames:v 1 -y $(ASSET_DIR)/clownfish.png
	ffmpeg -i $(FISH_DIR)/fish3Texture.png -vf "hflip" -frames:v 1 -y $(ASSET_DIR)/angelfish.png
	ffmpeg -i $(FISH_DIR)/fish4Texture.png -vf "hflip" -frames:v 1 -y $(ASSET_DIR)/barracuda.png
	ffmpeg -i $(FISH_DIR)/sardine-no-back.png -vf "scale=80:-1"  -frames:v 1 -y $(ASSET_DIR)/sardine.png

bear:
	bear -- make clean all

run: $(TARGET)
	./$(TARGET)

install-conf: aquarium.conf
	mkdir -p $(HOME)/.config/gtkaqua
	cp aquarium.conf $(HOME)/.config/gtkaqua/aquarium.conf
	@echo "Installed aquarium.conf to $(HOME)/.config/gtkaqua/aquarium.conf"

clean:
	rm -rf $(BUILD_DIR) aquarium
