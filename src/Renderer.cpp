#include "Renderer.hpp"
#include "labhelper.h"

Renderer::Renderer() {
  g_window = labhelper::init_window_SDL("PathTracer", 1280, 720);
}

void Renderer::initialize() {
  ///////////////////////////////////////////////////////////////////////////
  // Load shader program
  ///////////////////////////////////////////////////////////////////////////
  shaderProgram = labhelper::loadShaderProgram(
      "../pathtracer/copyTexture.vert", "../pathtracer/copyTexture.frag");
  simpleShaderProgram = labhelper::loadShaderProgram(
      "../pathtracer/simple.vert", "../pathtracer/simple.frag");

  ///////////////////////////////////////////////////////////////////////////
  // Generate result texture
  ///////////////////////////////////////////////////////////////////////////
  glGenTextures(1, &pathtracer_result_txt_id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, pathtracer_result_txt_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  ///////////////////////////////////////////////////////////////////////////
  // Initial path-tracer settings
  ///////////////////////////////////////////////////////////////////////////
  pathtracer::settings.max_bounces = 8;
  pathtracer::settings.max_paths_per_pixel = 0; // 0 = Infinite

  pathtracer::settings.spp = 4;
  pathtracer::settings.reflection_strength = 0.3f;
#ifdef _DEBUG
  pathtracer::settings.subsampling = 16;
#else
  pathtracer::settings.subsampling = 4;
#endif

  ///////////////////////////////////////////////////////////////////////////
  // Set up light sources
  ///////////////////////////////////////////////////////////////////////////
  pathtracer::point_light.intensity_multiplier = 2500.0f;
  pathtracer::point_light.color = vec3(1.f, 1.f, 1.f);
  pathtracer::point_light.position = vec3(10.0f, 25.0f, 20.0f);

  // float intensity_multiplier;
  // vec3 color;
  // vec3 position;
  // vec3 direction;
  // float radius;
  /*
  pathtracer::disc_lights.push_back( pathtracer::DiscLight{
                                                                     1000,
                                                                     {1, 0.8,
  0},
                                                                     {-8, 10,
  8}, glm::normalize(glm::vec3(10, -2, 10)), 8.0 } );
  pathtracer::disc_lights.push_back( pathtracer::DiscLight{
                                                                     1000,
                                                                     {0.1, 0.3,
  1},
                                                                     {-10, 20,
  -5}, glm::normalize(-glm::vec3(-10, 20, -5)), 10.0 } );
  */

  ///////////////////////////////////////////////////////////////////////////
  // Load environment map
  ///////////////////////////////////////////////////////////////////////////
  pathtracer::environment.map.load("../scenes/envmaps/001.hdr");
  pathtracer::environment.multiplier = 1.0f;

  ///////////////////////////////////////////////////////////////////////////
  // Load .obj models to scene
  ///////////////////////////////////////////////////////////////////////////
  loadScenes();
  changeScene("Ship");
  // changeScene("Sphere");
  // changeScene("Refractions");

  ///////////////////////////////////////////////////////////////////////////
  // This is INCORRECT! But an easy way to get us a brighter image that
  // just looks a little better...
  ///////////////////////////////////////////////////////////////////////////
  // glEnable(GL_FRAMEBUFFER_SRGB);
}
