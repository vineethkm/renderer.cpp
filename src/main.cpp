#include "Renderer.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  Renderer renderer;
  renderer.init();
  renderer.render();

  std::cout << "hello world" << std::endl;
  return 0;
}
