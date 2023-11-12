#include "softwarerenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "core/log.h"

SoftwareRenderer::SoftwareRenderer(SDL_Renderer* sdlRendererIn) :
	sdlRenderer(sdlRendererIn),
	sdlTexture(nullptr),
	rawPixels(nullptr),
	WIDTH(0),
    HEIGHT(0)
{
}

SoftwareRenderer::~SoftwareRenderer()
{
	Destroy();
}

void SoftwareRenderer::Destroy()
{
	if (sdlTexture)
	{
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = nullptr;
	}
	if (rawPixels)
    {
        delete [] rawPixels;
		rawPixels = nullptr;
    }
}

// viewport = (x, y, width, height)
void SoftwareRenderer::Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
                            const glm::vec4& viewport, const glm::vec2& nearFar)
{
}

// viewport = (x, y, width, height)
void SoftwareRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                              const glm::vec4& viewport, const glm::vec2& nearFar)
{
	if (WIDTH != (int)viewport.z || HEIGHT != (int)viewport.w)
	{
		Resize(viewport);
	}
	assert(rawPixels);

    RenderImpl(cameraMat, projMat, viewport, nearFar);

	SDL_UpdateTexture(sdlTexture, NULL, rawPixels, WIDTH * sizeof(uint32_t));
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
}

void SoftwareRenderer::RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
                                  const glm::vec4& viewport, const glm::vec2& nearFar)
{
	const int PIXEL_STEP = 10;
	for (int y = 0; y < HEIGHT; y += PIXEL_STEP)
    {
		for (int x = 0; x < WIDTH; x += PIXEL_STEP)
        {
			int r = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
			int g = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
			int b = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
            for (int xx = 0; xx < PIXEL_STEP; xx++)
            {
                for (int yy = 0; yy < PIXEL_STEP; yy++)
                {
					SetPixel(x + xx, y + yy, r, g, b);
                }
            }
		}
	}
}

void SoftwareRenderer::Resize(const glm::vec4& newViewport)
{
	Destroy();
	const int newWidth = (int)newViewport.z;
	const int newHeight = (int)newViewport.w;
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, newWidth, newHeight);
    rawPixels = new uint32_t[newWidth * newHeight];
	WIDTH = newWidth;
    HEIGHT = newHeight;
}

void SoftwareRenderer::SetPixel(int x, int y, int r, int g, int b)
{
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
	{
        //rawPixels[y * WIDTH + x] = (uint32_t)((r << 16) + (g << 8) + (b << 0));
		rawPixels[((HEIGHT - 1) - y) * WIDTH + x] = (uint32_t)((r << 16) + (g << 8) + (b << 0));
	}
}

void SoftwareRenderer::SetThickPixel(int x, int y, int r, int g, int b)
{
    SetPixel(x, y, r, g, b);
    SetPixel(x + 1, y, r, g, b);
    SetPixel(x + 1, y + 1, r, g, b);
    SetPixel(x, y + 1, r, g, b);
}

void SoftwareRenderer::ClearPixels()
{
    memset(rawPixels, 0, sizeof(uint32_t) * WIDTH * HEIGHT);
}
