cmake_minimum_required(VERSION 3.10)
project(CG_Fase2)

set(CMAKE_CXX_STANDARD 17)

# Generator
add_executable(generator generator/main.cpp)

# Engine
add_executable(engine 
    engine/engine.cpp 
    engine/tinyxml2.cpp 
    engine/tinyxml2.h
)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIR} engine/)
target_link_libraries(engine PRIVATE ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})