# Minimal Makefile to build the renderer using system Embree 4

CXX := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -Iinc -Ilabhelper -Ilib/imgui -Ilib/stb -Ilib/tinyobjloader -Ilib/embree/include -DGLM_ENABLE_EXPERIMENTAL

# Try to get SDL2 flags via pkg-config if available
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

SOURCES = $(shell find src labhelper -name '*.cpp')
SOURCES += lib/imgui/imgui.cpp lib/imgui/imgui_draw.cpp
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))

LIBS := $(SDL_LIBS) -lGLEW -lGL -lGLU -lembree4 -lpthread -ldl -lm

all: renderer

renderer: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) renderer

.PHONY: all clean

