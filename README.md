# Path Tracing Renderer

## Overview

This project is a rendering system developed in C++ as part of an advanced computer graphics course project. It is built using the Chalmers University Computer Graphics framework as a starting point and is being progressively extended with rendering and optimization features.

The renderer focuses on implementing and exploring ray tracing and path tracing techniques, with planned support for acceleration structures, multithreaded rendering, and GPU-based rendering pipelines.

## Project Structure

```text
path-tracing-project/
│
├── pathtracer/        # Core renderer source code
├── labhelper/         # Utility/helper framework
├── scenes/            # Models, HDR maps, scene assets
├── external/          # External prebuilt libraries
├── external_src/      # External source dependencies
│
├── .gitignore
├── CMakeLists.txt     # CMake build configuration
├── VSUserTemplate.user
├── README.md
└── README_LINUX.md
```

## Important
For best rendering performance, run the project using the `Release` build configuration.
