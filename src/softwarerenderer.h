#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"

struct SDL_Renderer;
struct SDL_Texture;

class SoftwareRenderer
{
public:
    SoftwareRenderer(SDL_Renderer* sdlRendererIn);
    virtual ~SoftwareRenderer();

	// viewport = (x, y, width, height)
	void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
			  const glm::vec4& viewport, const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
				const glm::vec4& viewport, const glm::vec2& nearFar);

protected:
	virtual void RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
							const glm::vec4& viewport, const glm::vec2& nearFar);

	void Destroy();
	void Resize(const glm::vec4& newViewport);
	void SetPixel(int x, int y, int r, int g, int b);
	void SetThickPixel(int x, int y, int r, int g, int b);
	void ClearPixels();

	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	uint32_t* rawPixels;
	int WIDTH;
	int HEIGHT;
};
