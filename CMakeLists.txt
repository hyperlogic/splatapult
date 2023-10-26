# mkdir build
# cd build
# cmake -DCMAKE_TOOLCHAIN_FILE="C:\msys64\home\ajthy\code2\vcpkg\scripts\buildsystems\vcpkg.cmake" ..
# cmake --build .

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(3dgstoy LANGUAGES CXX)

set(PROJECT_NAME 3dgstoy)

# opengl
find_package(OpenGL REQUIRED)
include_directories(${GL_INCLUDE_DIRS})

# sdl2
find_package(SDL2 CONFIG REQUIRED)

# glew
find_package(GLEW REQUIRED)

# glm
find_package(glm REQUIRED)

# png
find_package(PNG REQUIRED)

# nlohmann-json
find_package(nlohmann_json CONFIG REQUIRED)

# tracy
find_package(Tracy CONFIG REQUIRED)
#add_compile_definitions(TRACY_ENABLE)

# openxr-loader
find_package(OpenXR CONFIG REQUIRED)

# src
include_directories(src)
add_executable(${PROJECT_NAME}
    src/cameras.cpp
    src/debugdraw.cpp
    src/flycam.cpp
    src/gaussiancloud.cpp
    src/image.cpp
    src/joystick.cpp
    src/log.cpp
    src/main.cpp
    src/pointcloud.cpp
    src/pointrenderer.cpp
    src/program.cpp
    src/raymarchrenderer.cpp
    src/texture.cpp
    src/splatrenderer.cpp
    src/util.cpp
    src/vertexbuffer.cpp
    src/xrbuddy.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(WIN32)
    # Comment this out to see SDL_Log output for debugging
    # set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENGL_LIBRARIES}
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    GLEW::GLEW
    glm::glm
    PNG::PNG
    nlohmann_json::nlohmann_json
    Tracy::TracyClient
    OpenXR::headers
    OpenXR::openxr_loader
)



