CC := g++
FLAGS := -Wall -std=c++20 -march=native -DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ONE
DEBUG_FLAGS = -p -g3
RELEASE_FLAGS = -Ofast -flto -DNDEBUG
INCLUDE := -Isrc  -Iinclude -Iinclude/vulkan -Iinclude/imgui
LINK := -lvulkan `pkg-config sdl3 --libs`

CCFLAGS := $(FLAGS) $(INCLUDE) $(LINK)
LDFLAGS := $(FLAGS)

BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
VENDOR_PATH := vendor

TARGET_NAME := pt
TARGET := $(BIN_PATH)/$(TARGET_NAME)

SRC := $(shell find $(SRC_PATH)/ -name "*.cpp")
VENDOR := $(shell find $(VENDOR_PATH)/ -name "*.cpp")
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

CLEAN_LIST := $(TARGET) $(OBJ)

default: release

.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH)

.PHONY: release
release: makedir
	$(CC) -o $(TARGET) $(SRC) $(VENDOR) $(CCFLAGS) $(RELEASE_FLAGS)

.PHONY: debug
debug: makedir
	$(CC) -o $(TARGET) $(SRC) $(VENDOR) $(CCFLAGS) $(DEBUG_FLAGS)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)