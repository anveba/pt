SRC_DIR = src vendor
OBJ_DIR = obj
OUT_DIR = bin
SHADER_DIR = shaders 

CXX = g++
DXC = dxc

CXX_FLAGS := -Wall -std=c++20 -march=native -DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ONE
CXX_DEBUG_FLAGS = -p -g3 -DDEBUG
CXX_RELEASE_FLAGS = -O2 -flto -DNDEBUG
CXX_INCLUDE := -Isrc -Iinclude -Iinclude/vulkan -Iinclude/imgui
CXX_LINK := -Llib -lvulkan -lSDL3 -lassimp
DXC_FLAGS = -spirv -E main -fspv-target-env=vulkan1.3 -Wall -O3

TARGET_NAME := pt
TARGET := $(OUT_DIR)/$(TARGET_NAME)

CPP_SRC = $(shell find $(SRC_DIR) -name '*.cpp')
CPP_OBJECTS = $(CPP_SRC:%=$(OBJ_DIR)/%.o)

SHADER_SRC = $(shell find $(SHADER_DIR) -name '*.hlsl')
SHADER_SPV = $(SHADER_SRC:%.hlsl=$(OUT_DIR)/%.spv)

all: release

debug: CXX_FLAGS += $(CXX_DEBUG_FLAGS)
debug: $(TARGET) $(SHADER_SPV)

release: CXX_FLAGS += $(CXX_RELEASE_FLAGS)
release: $(TARGET) $(SHADER_SPV)

$(TARGET): $(CPP_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPP_OBJECTS) -o $@ $(CXX_LINK)

$(OBJ_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDE) -c $< -o $@

$(OUT_DIR)/%.spv: %.hlsl
	@mkdir -p $(dir $@)
	$(DXC) $(DXC_FLAGS) -T $(call get_shader_model,$(notdir $<)) -Fo $@ $<

get_shader_model = \
	$(if $(findstring .vs,$1),vs_6_0, \
	$(if $(findstring .ps,$1),ps_6_0, \
	$(if $(findstring .cs,$1),cs_6_0, \
	lib_6_3)))

clean:
	rm -rf $(OBJ_DIR) $(OUT_DIR)

.PHONY: all debug release clean