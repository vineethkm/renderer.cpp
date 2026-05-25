#pragma once
#include <GL/glew.h>

class Renderer {
private:
  // Window Atributes
  SDL_Window *g_window = nullptr;
  int windowsWdith = 0, windowHeight = 0;

  // Time
  float currentTime = 0.0f;
  float deltaTime = 0.0f;

  Gluint shaderProgram;
  Gluint simpleShaderProgram;

public:
  Renderer();
  ~Renderer();

  void init();
  // Load Shaders, environment maps, models & ...
  void initialize(void);
  void display(void);
  bool handleEvents(void);
  void gui(void);
  void renderLoop(void);
  void loadScenes(void);
  void changeScene(void);
  void cleanupScenes(void);
};
