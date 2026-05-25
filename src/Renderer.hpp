#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>
#include <Model.h>

using namespace glm;

class Renderer {
private:
  // Window Atributes
  // SDL_Window *g_window = nullptr;
  int windowsWdith = 0, windowHeight = 0;

  // Time
  float currentTime = 0.0f;
  float deltaTime = 0.0f;

  // Shader Programs
  GLuint shaderProgram;
  GLuint simpleShaderProgram;

  //  GL texture to put pathtracing result into
  uint32_t pathtracer_result_txt_id;

  // flags
  bool showUI = true;
  bool showLightSources = false;

  // Scenes
  vec3 worldUp = vec3(0.0f, 1.0f, 0.0f);

  // Camera Struct
  struct camera_t {
    vec3 position;
    vec3 direction;
  };

  // Scene Structs
  struct scene_t {
    struct scene_object_t {
      labhelper::Model *model;
      mat4 modelMat;
    };
    std::vector<scene_object_t> models;

    camera_t camera;
  };

  // Multiple Scenes to switch from
  std::map<std::string, scene_t> scenes;
  std::string currentScene;
  camera_t camera;

  int selected_model_index = 0;
  int selected_mesh_index = 0;
  int selected_material_index = 0;

public:
  Renderer();
  ~Renderer();

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
