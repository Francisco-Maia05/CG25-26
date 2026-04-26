cmake_minimum_required(VERSION 3.10)
project(CG_Fase3 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------
find_package(OpenGL REQUIRED)
find_package(GLUT   REQUIRED)
find_package(GLEW   REQUIRED)

# --------------------------------------------------------------------------
# Generator  (no OpenGL/GLUT dependency)
# --------------------------------------------------------------------------
add_executable(generator
    generator/main.cpp
)
target_compile_definitions(generator PRIVATE _USE_MATH_DEFINES)

# --------------------------------------------------------------------------
# Engine
# --------------------------------------------------------------------------
add_executable(engine
    engine/engine.cpp
    engine/tinyxml2.cpp      # compiled source
)

target_include_directories(engine PRIVATE
    ${OPENGL_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    engine/                  # so tinyxml2.h is found
)

target_link_libraries(engine PRIVATE
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    GLEW::GLEW
)

# On Windows with freeglut the entry point may need this:
if(WIN32)
    target_link_libraries(engine PRIVATE winmm)
endif()