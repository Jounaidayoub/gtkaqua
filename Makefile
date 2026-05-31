CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

PKG_CONFIG ?= pkg-config

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4)
GTK_LIBS   := $(shell $(PKG_CONFIG) --libs gtk4)

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin
TARGET    := $(BIN_DIR)/aquarium

APP_SRC := $(wildcard src/*.c)
APP_OBJ := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(APP_SRC))
TOML_OBJ := $(OBJ_DIR)/toml.o

FISH_DIR  := Fish
ASSET_DIR := assets

.PHONY: all clean assets bear run

all: $(TARGET)

$(TARGET): $(APP_OBJ) $(TOML_OBJ)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(APP_OBJ) $(TOML_OBJ) -o $@ $(GTK_LIBS) -lm

$(OBJ_DIR)/%.o: src/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ithird_party/tomlc99 $(GTK_CFLAGS) -c $< -o $@

$(TOML_OBJ): third_party/tomlc99/toml.c
	mkdir -p $(OBJ_DIR)
	$(CC) -O2 -c $< -o $@

assets:
	mkdir -p $(ASSET_DIR)
	ffmpeg -i $(FISH_DIR)/shark.webp        -vf "scale=100:-1"  -frames:v 1 -y $(ASSET_DIR)/shark.png
	ffmpeg -i $(FISH_DIR)/bass.webp         -vf "scale=80:-1"   -frames:v 1 -y $(ASSET_DIR)/bass.png
	ffmpeg -i $(FISH_DIR)/trout.webp        -vf "scale=80:-1"   -frames:v 1 -y $(ASSET_DIR)/trout.png
	ffmpeg -i $(FISH_DIR)/tuna.webp         -vf "scale=80:-1"   -frames:v 1 -y $(ASSET_DIR)/tuna.png
	ffmpeg -i $(FISH_DIR)/fish1Texture.png  -vf "hflip"         -frames:v 1 -y $(ASSET_DIR)/goldfish.png
	ffmpeg -i $(FISH_DIR)/fish2Texture.png  -vf "hflip"         -frames:v 1 -y $(ASSET_DIR)/clownfish.png
	ffmpeg -i $(FISH_DIR)/fish3Texture.png  -vf "hflip"         -frames:v 1 -y $(ASSET_DIR)/angelfish.png
	ffmpeg -i $(FISH_DIR)/fish4Texture.png  -vf "hflip"         -frames:v 1 -y $(ASSET_DIR)/barracuda.png

bear:
	bear -- make clean all

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) aquarium
