# Minimal Makefile to build the renderer using system Embree 4

CXX := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -Iinc -Ilabhelper -Ilib/imgui -Ilib/stb -Ilib/tinyobjloader -DGLM_ENABLE_EXPERIMENTAL

# Try to get SDL2 flags via pkg-config if available
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

BUILD_DIR = build

SOURCES = $(shell find src labhelper -name '*.cpp')
SOURCES += lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp lib/imgui/imgui_widgets.cpp 
SOURCES += lib/imgui/imgui_tables.cpp lib/imgui/backends/imgui_impl_sdl2.cpp lib/imgui/backends/imgui_impl_opengl3.cpp
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

LIBS := $(SDL_LIBS) -lGLEW -lGL -lGLU -lembree4 -lpthread -ldl -lm

all: renderer

renderer: build $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LIBS)

build:
	@mkdir -p build

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

clean:
	rm -rf build renderer

.PHONY: all clean

