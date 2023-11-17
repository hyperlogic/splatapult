#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"
#include "softwarerenderer.h"

struct GeomOut;

class ShaderDebugRenderer : public SoftwareRenderer
{
public:
    ShaderDebugRenderer(SDL_Renderer* sdlRendererIn);
    virtual ~ShaderDebugRenderer();

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud);

    // viewport = (x, y, width, height)
    virtual void RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
							const glm::vec4& viewport, const glm::vec2& nearFar) override;

protected:
    void SampleTri(const GeomOut& p0, const GeomOut& p1, const GeomOut& p2);

    std::vector<glm::vec4> posVec;
    std::vector<glm::vec4> r_sh0Vec, g_sh0Vec, b_sh0Vec;
    std::vector<glm::vec3> cov3_col0Vec, cov3_col1Vec, cov3_col2Vec;
};
