cmake_minimum_required(VERSION 3.10)
project(engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(engine engine.cpp)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

target_include_directories(engine PRIVATE ${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
target_link_libraries(engine PRIVATE ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
