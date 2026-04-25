cmake_minimum_required(VERSION 3.10)
project(CG_Fase3)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)

include_directories(
    ${OPENGL_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    engine/
)

# Generator
add_executable(generator generator/main.cpp)

# Engine
add_executable(engine
    engine/engine.cpp
    engine/tinyxml2.cpp
    engine/tinyxml2.h
)

target_link_libraries(engine PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    GLEW::GLEW
)