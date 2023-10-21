#include "raymarchrenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "log.h"

RayMarchRenderer::RayMarchRenderer()
{
}

RayMarchRenderer::~RayMarchRenderer()
{
}

bool RayMarchRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    gVec.reserve(gaussianCloud->GetGaussianVec().size());
    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        const float SH_C0 = 0.28209479177387814f;
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        glm::vec3 color(0.5f + SH_C0 * g.f_dc[0],
                        0.5f + SH_C0 * g.f_dc[1],
                        0.5f + SH_C0 * g.f_dc[2]);
        Gaussian gg(glm::vec3(g.position[0], g.position[1], g.position[2]),
                    g.ComputeCovMat(),
                    color, alpha);
        gVec.push_back(gg);
    }

    return true;
}

// viewport = (x, y, width, height)
void RayMarchRenderer::Render(const glm::mat4& cameraMat, const glm::vec4& viewport,
                              const glm::vec2& nearFar, float fovy, SDL_Renderer* renderer)
{
    float width = viewport.z;
    float height = viewport.w;
    float aspect = width / height;

    glm::vec3 eye = glm::vec3(cameraMat[3]);
    glm::mat3 cameraMat3 = glm::mat3(cameraMat);

    const int PIXEL_STEP = 4;
    const float theta0 = fovy / 2.0f;
    const float dTheta = -fovy / (float)height;
    for (int y = 0; y < height; y += PIXEL_STEP)
    {
        float theta = theta0 + dTheta * y;
        float phi0 = -fovy * aspect / 2.0f;
        float dPhi = (fovy * aspect) / width;
        for (int x = 0; x < width; x += PIXEL_STEP)
        {
            float phi = phi0 + dPhi * x;
            glm::vec3 localRay(sinf(phi), sinf(theta), -cosf(theta));
            glm::vec3 ray = glm::normalize(cameraMat3 * localRay);

            // integrate along the ray, (midpoint rule)
            const float RAY_LENGTH = 6.0f;
            const int NUM_STEPS = 50;
            float t0 = 0.1f;
            float dt = (RAY_LENGTH - t0) / NUM_STEPS;
            glm::vec3 accum(0.0f, 0.0f, 0.0f);
            for (int i = 0; i < NUM_STEPS; i++)
            {
                float t = t0 + i * dt;
                for (auto&& g : gVec)
                {
                    float gg = g.Eval(eye + ray * t);
                    accum += g.color * (gg * dt);
                }
            }

            uint8_t r = (uint8_t)(glm::clamp(accum.x, 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(glm::clamp(accum.y, 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(glm::clamp(accum.z, 0.0f, 1.0f) * 255.0f);
            //uint8_t a = (uint8_t)(glm::clamp(accum, 0.0f, 1.0f) * 255.0f);

            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            for (int xx = 0; xx < PIXEL_STEP; xx++)
            {
                for (int yy = 0; yy < PIXEL_STEP; yy++)
                {
                    SDL_RenderDrawPoint(renderer, x + xx, y + yy);
                }
            }
        }
    }
}
