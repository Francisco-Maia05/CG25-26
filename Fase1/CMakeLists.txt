cmake_minimum_required(VERSION 3.10)
project(cg_phase1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generator
add_executable(generator generator/main.cpp)

# Engine (OpenGL + GLUT)
add_executable(engine engine/engine.cpp)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

target_link_libraries(engine PRIVATE OpenGL::GL OpenGL::GLU GLUT::GLUT)