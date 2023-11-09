#ifndef SOFTWARESPLATRENDERER_H
#define SOFTWARESPLATRENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"

struct SDL_Renderer;
struct SDL_Texture;

class SoftwareSplatRenderer
{
public:
    SoftwareSplatRenderer(SDL_Renderer* sdlRendererIn);
    ~SoftwareSplatRenderer();

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud);

	// viewport = (x, y, width, height)
	void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
              const glm::vec4& viewport, const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

protected:
	void Destroy();
	void Resize(const glm::vec4& newViewport);

	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	uint32_t* rawPixels;
	glm::vec4 prevViewport;
};

#endif
