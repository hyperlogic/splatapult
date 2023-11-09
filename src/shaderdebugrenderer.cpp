#include "shaderdebugrenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "core/log.h"

ShaderDebugRenderer::ShaderDebugRenderer(SDL_Renderer* sdlRendererIn) : SoftwareRenderer(sdlRendererIn)
{
}

ShaderDebugRenderer::~ShaderDebugRenderer()
{
}

bool ShaderDebugRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    return true;
}

// viewport = (x, y, width, height)
void ShaderDebugRenderer::RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
                                     const glm::vec4& viewport, const glm::vec2& nearFar)
{
    const int WIDTH = (int)viewport.z;
    const int HEIGHT = (int)viewport.w;

    SoftwareRenderer::RenderImpl(cameraMat, projMat, viewport, nearFar);
}
