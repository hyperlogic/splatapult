# See BUILD.md for more info
# Windows cheat sheet
# > mkdir build
# > cd build
# > cmake -DSHIPPING=ON -DCMAKE_TOOLCHAIN_FILE="C:\msys64\home\hyperlogic\code\vcpkg\scripts\buildsystems\vcpkg.cmake" ..
# > cmake --build . --config Release

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(PROJECT_NAME splatapult)
project(${PROJECT_NAME} LANGUAGES CXX)

option(SHIPPING "Build for shipping" OFF)

if(UNIX)
    find_package(X11 REQUIRED)
endif()

if(WIN32)
    # kind of a hack, I want to be able to include glm/glm.hpp on windows and linux
    include_directories(${VCPKG_INSTALLED_DIR}/x64-windows/include)
endif()

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
if (WIN32)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()

# eigen
find_package(Eigen3 CONFIG REQUIRED)

# tracy
if(WIN32 AND NOT SHIPPING)
    find_package(Tracy CONFIG REQUIRED)
    add_compile_definitions(TRACY_ENABLE)
    set(TRACY_LIBRARIES, Tracy::TracyClient)
else()
    set(TRACY_LIBRARIES, "")
endif()

# openxr-loader
find_package(OpenXR CONFIG REQUIRED)

# src
include_directories(src)
add_executable(${PROJECT_NAME}
    src/core/binaryattribute.cpp
    src/core/debugrenderer.cpp
    src/core/framebuffer.cpp
    src/core/image.cpp
    src/core/inputbuddy.cpp
    src/core/log.cpp
    src/core/program.cpp
    src/core/texture.cpp
    src/core/util.cpp
    src/core/vertexbuffer.cpp
    src/core/textrenderer.cpp
    src/core/xrbuddy.cpp

    src/app.cpp
    src/camerasconfig.cpp
    src/camerapathrenderer.cpp
    src/flycam.cpp
    src/gaussiancloud.cpp
    src/magiccarpet.cpp
    src/ply.cpp
    src/pointcloud.cpp
    src/pointrenderer.cpp
    src/sdl_main.cpp
    src/splatrenderer.cpp
    src/vrconfig.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(WIN32)
    # Comment this out to see SDL_Log output for debugging
    # set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)
endif()

if(WIN32)
    if(SHIPPING)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${OPENGL_LIBRARIES}
            $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
            $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
            GLEW::GLEW
            glm::glm
            PNG::PNG
            nlohmann_json::nlohmann_json
            Eigen3::Eigen
            OpenXR::headers
            OpenXR::openxr_loader
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${OPENGL_LIBRARIES}
            $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
            $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
            GLEW::GLEW
            glm::glm
            PNG::PNG
            nlohmann_json::nlohmann_json
            Eigen3::Eigen
            Tracy::TracyClient
            OpenXR::headers
            OpenXR::openxr_loader
        )
    endif()
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${OPENGL_LIBRARIES}
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        GLEW::GLEW
        glm::glm
        PNG::PNG
        # nlohmann_json::nlohmann_json
        Eigen3::Eigen
        OpenXR::headers
        OpenXR::openxr_loader
        ${X11_LIBRARIES}
    )
endif()

if(SHIPPING)
    add_compile_definitions(SHIPPING)

    # Copy required data directories to the executable directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/font
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/font
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/shader
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/shader
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/texture
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/texture
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/data/test.ply
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/data/test.ply
    )
endif()