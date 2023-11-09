#include "softwaresplatrenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "core/log.h"

static void SetPixel(uint32_t* rawPixels, int WIDTH, int HEIGHT, int x, int y, int r, int g, int b)
{
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
	{
		rawPixels[y * WIDTH + x] = (uint32_t)((r << 16) + (g << 8) + (b << 0));
	}
}

SoftwareSplatRenderer::SoftwareSplatRenderer(SDL_Renderer* sdlRendererIn) :
	sdlRenderer(sdlRendererIn),
	sdlTexture(nullptr),
	rawPixels(nullptr),
	prevViewport(0.0f, 0.0f, 0.0f, 0.0f)
{
}

SoftwareSplatRenderer::~SoftwareSplatRenderer()
{
	Destroy();
}

bool SoftwareSplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    return true;
}

void SoftwareSplatRenderer::Destroy()
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
void SoftwareSplatRenderer::Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
								 const glm::vec4& viewport, const glm::vec2& nearFar)
{
}

// viewport = (x, y, width, height)
void SoftwareSplatRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
								   const glm::vec4& viewport, const glm::vec2& nearFar)
{
	if (prevViewport != viewport)
	{
		Resize(viewport);
	}

	assert(rawPixels);
	const int WIDTH = (int)viewport.z;
	const int HEIGHT = (int)viewport.w;
	const int PIXEL_STEP = 10;
	for (int y = 0; y < HEIGHT; y += PIXEL_STEP)
    {
		for (int x = 0; x < WIDTH; x += PIXEL_STEP)
        {
			int r = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
			int g = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
			int b = static_cast<int>((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 255.0f);
			//SDL_SetRenderDrawColor(sdlRenderer, r, g, b, 255);
            for (int xx = 0; xx < PIXEL_STEP; xx++)
            {
                for (int yy = 0; yy < PIXEL_STEP; yy++)
                {
					SetPixel(rawPixels, WIDTH, HEIGHT, x + xx, y + yy, r, g, b);
                    //SDL_RenderDrawPoint(sdlRenderer, x + xx, y + yy);
                }
            }
		}
	}

	SDL_UpdateTexture(sdlTexture, NULL, rawPixels, WIDTH * sizeof(uint32_t));
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
}

void SoftwareSplatRenderer::Resize(const glm::vec4& newViewport)
{
	Destroy();
	const int newWidth = (int)newViewport.z;
	const int newHeight = (int)newViewport.w;
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, newWidth, newHeight);
    rawPixels = new uint32_t[newWidth * newHeight];
	prevViewport = newViewport;
}