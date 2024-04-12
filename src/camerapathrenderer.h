/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "core/program.h"
#include "core/vertexbuffer.h"

struct Camera;

class CameraPathRenderer
{
public:
    CameraPathRenderer();
    ~CameraPathRenderer();

    bool Init(const std::vector<Camera>& cameraVec);

    void SetShowCameras(bool showCamerasIn) { showCameras = showCamerasIn; }
    void SetShowPath(bool showPathIn) { showPath = showPathIn; }

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

protected:
    void BuildCamerasVao(const std::vector<Camera>& cameraVec);
    void BuildPathVao(const std::vector<Camera>& cameraVec);

    std::shared_ptr<Program> ddProg;
    std::shared_ptr<VertexArrayObject> camerasVao;
    uint32_t numCameraVerts;
    std::shared_ptr<VertexArrayObject> pathVao;
    uint32_t numPathVerts;

    bool showCameras = true;
    bool showPath = true;
};
