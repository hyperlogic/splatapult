#ifndef SHADERDEBUGRENDERER_H
#define SHADERDEBUGRENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"
#include "softwarerenderer.h"

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

};

#endif
