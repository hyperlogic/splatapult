#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "core/program.h"
#include "core/vertexbuffer.h"

#include "gaussiancloud.h"

// depth peeling splat renderer
class DPSplatRenderer
{
public:
    DPSplatRenderer();
    ~DPSplatRenderer();

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud, bool isFramebufferSRGBEnabledIn,
              bool useFullSHIn);

    void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
              const glm::vec4& viewport, const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);
protected:
    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);
    void FrameBufferInit(int width, int height);

    std::shared_ptr<Program> splatProg;
    std::shared_ptr<Program> desktopProg;
    std::shared_ptr<VertexArrayObject> splatVao;

    std::vector<glm::vec4> posVec;

    glm::ivec2 fboSize;
    uint32_t fbo;
    uint32_t fboTex;
    uint32_t fboRbo;

    bool isFramebufferSRGBEnabled;
    bool useFullSH;
};
