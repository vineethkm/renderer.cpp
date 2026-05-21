# Linux Build Instructions

## Note

This project is primarily developed and tested on Windows with Visual Studio 2022. Linux support is intended for users familiar and comfortable with a Linux development environment.

## Description

The project uses CMake as its build system. You will need to install CMake along with the required third-party libraries.

Required dependencies include:

* SDL2
* GLEW
* GLM
* OpenGL implementation (e.g. Mesa)
* Embree2

Install the required packages using your package manager of choice:

```shell
apt-get install ...
yum install ...
pacman -S ...
...
```

If any required libraries are missing, CMake will report errors during configuration.

## Build Instructions

Navigate to the project root directory and create a build folder:

```shell
mkdir build
cd build
```

Generate build files using CMake:

```shell
cmake ..
```

or for a Release build:

```shell
cmake -DCMAKE_BUILD_TYPE=Release ..
```

Build the project using:

```shell
make
```

The executable will be generated inside the build output directory.