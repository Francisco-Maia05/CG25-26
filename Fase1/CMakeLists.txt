cmake_minimum_required(VERSION 3.10)
project(CG_Phase1)

set(CMAKE_CXX_STANDARD 17)

# Encontrar pacotes necessários para o Engine
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

# --- GENERATOR ---
add_executable(generator generator/main.cpp)

# --- ENGINE ---
# Inclui os ficheiros do TinyXML2 se os baixares, ou usa uma lib do sistema
add_executable(engine engine/main.cpp) 
target_link_libraries(engine ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})