# mkdir build
# cd build
# cmake -DCMAKE_PREFIX_PATH=../../deps/SDL2-2.28.3 ..


cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(3dgstoy LANGUAGES CXX)

set(PROJECT_NAME 3dgstoy)

find_package(OpenGL REQUIRED)
include_directories(${GL_INCLUDE_DIRS})

find_package(SDL2 REQUIRED)
get_target_property(SDL2_INCLUDE_DIRS SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} src/main.cpp)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES} SDL2::SDL2 SDL2::SDL2main)


