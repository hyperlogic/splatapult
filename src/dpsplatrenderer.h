#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "core/program.h"
#include "core/texture.h"
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
    void FrameBufferInit();
    void ComposeLayers();

    std::shared_ptr<Program> splatProg;
    std::shared_ptr<Program> peelProg;
    std::shared_ptr<Program> desktopProg;
    std::shared_ptr<VertexArrayObject> splatVao;

    std::vector<glm::vec4> posVec;

    glm::ivec2 fboSize;
    std::vector<uint32_t> fboVec;
    std::vector<uint32_t> rboVec;
    std::vector<std::shared_ptr<Texture>> colorTexVec;
    std::vector<std::shared_ptr<Texture>> depthTexVec;

    // AJT: HACK REMOVE
    std::shared_ptr<Texture> carpetTex;

    bool isFramebufferSRGBEnabled;
    bool useFullSH;
};
