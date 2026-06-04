CXX      := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -Iinc -Ilabhelper -Ilib/imgui -Ilib/stb -Ilib/tinyobjloader -I/usr/include/SDL2 -DGLM_ENABLE_EXPERIMENTAL

# SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := -lSDL2

BUILD_DIR := build

IMGUI_SRC := lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp \
             lib/imgui/imgui_tables.cpp lib/imgui/backends/imgui_impl_sdl2.cpp \
             lib/imgui/backends/imgui_impl_opengl3.cpp

# Embree Backend
ALL_SRC_EMBREE := $(filter-out src/CpuBVH.cpp, $(shell find src labhelper -name '*.cpp'))
SOURCES_EMBREE := $(ALL_SRC_EMBREE) $(IMGUI_SRC)
OBJECTS_EMBREE := $(patsubst %.cpp,$(BUILD_DIR)/embree_build/%.o,$(SOURCES_EMBREE))
LIBS_EMBREE    := $(SDL_LIBS) -lGLEW -lGL -lGLU -lembree4 -lpthread -ldl -lm

# CPU backed (exclude embree)
ALL_SRC_CPUBVH := $(filter-out src/embree.cpp, $(shell find src labhelper -name '*.cpp'))
SOURCES_CPUBVH := $(ALL_SRC_CPUBVH) $(IMGUI_SRC)
OBJECTS_CPUBVH := $(patsubst %.cpp,$(BUILD_DIR)/cpubvh_build/%.o,$(SOURCES_CPUBVH))
LIBS_CPUBVH    := $(SDL_LIBS) -lGLEW -lGL -lGLU -lpthread -ldl -lm

# -----------------------------------------------------------------------

all: renderer

renderer: $(OBJECTS_EMBREE)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS_EMBREE)

cpubvh: renderer_cpubvh

renderer_cpubvh: $(OBJECTS_CPUBVH)
	$(CXX) $(CXXFLAGS) -DCPUBVH -o $@ $^ $(LIBS_CPUBVH)

$(BUILD_DIR)/embree_build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/cpubvh_build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DCPUBVH -c $< -o $@

clean:
	rm -rf build renderer renderer_cpubvh

.PHONY: all cpubvh clean
