/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "raymarchrenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <tracy/Tracy.hpp>

#include "core/log.h"

RayMarchRenderer::RayMarchRenderer(SDL_Renderer* sdlRendererIn) : SoftwareRenderer(sdlRendererIn)
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

const size_t NUM_STEPS = 100;
struct Tau
{
    Tau(const glm::vec3& s, const glm::vec3& t, const RayMarchRenderer::Gaussian& g)
    {
        float accum = 0.0f;
        float dn = glm::length(t - s) / (float)NUM_STEPS;
        glm::vec3 n = glm::normalize(t - s);
        float dist = 0.0f;
        for (size_t i = 0; i < NUM_STEPS; i++)
        {
            dist += dn;
            tau[i] = g.Eval(s + n * dist) * dn;
            accum += tau[i];
            integral[i] = accum;
        }
    }

    // returns the area of a small slice of tau at step i
    float AtStep(size_t i) const
    {
        assert(i < NUM_STEPS);
        return tau[i];
    }

    // returns the sum of all the small slice areas from step 0 to step i.
    float IntegralFromZeroToStep(size_t i) const
    {
        assert(i < NUM_STEPS);
        return integral[i];
    }

    std::array<float, NUM_STEPS> tau;
    std::array<float, NUM_STEPS> integral;
};

static float RayIntegral(size_t k, const std::vector<Tau>& tauVec)
{
    float sum = 0.0f;
    for (size_t i = 0; i < NUM_STEPS; i++)
    {
        float product = 1.0f;
        for (size_t j = 0; j < tauVec.size(); j++)
        {
            // ignore self occlusion
            if (k == j)
            {
                continue;
            }
            product *= expf(-tauVec[j].IntegralFromZeroToStep(i));
        }
        sum += tauVec[k].AtStep(i) * product;
    }
    return sum;
}

// viewport = (x, y, width, height)
void RayMarchRenderer::RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
                                  const glm::vec4& viewport, const glm::vec2& nearFar)
{
    float width = viewport.z;
    float height = viewport.w;
    float aspect = width / height;

    glm::vec3 eye = glm::vec3(cameraMat[3]);
    glm::mat3 cameraMat3 = glm::mat3(cameraMat);

    // AJT: TODO use real proj matrix
    const float fovy = glm::radians(45.0f);

    const int PIXEL_STEP = 10;
    const float theta0 = fovy / 2.0f;
    const float dTheta = -fovy / (float)height;

    for (int y = 0; y < HEIGHT; y += PIXEL_STEP)
    {
        float theta = theta0 + dTheta * y;
        float phi0 = -fovy * aspect / 2.0f;
        float dPhi = (fovy * aspect) / width;
        for (int x = 0; x < WIDTH; x += PIXEL_STEP)
        {
            float phi = phi0 + dPhi * x;
            glm::vec3 localRay(sinf(phi), sinf(theta), -cosf(theta));
            glm::vec3 ray = glm::normalize(cameraMat3 * localRay);

            // ray march down each gaussian
            const float RAY_LENGTH = 3.0f;
            std::vector<Tau> tauVec;
            for (auto&& g : gVec)
            {
                tauVec.emplace_back(Tau(eye, ray * RAY_LENGTH, g));
            }

            glm::vec3 I(0.0f, 0.0f, 0.0f);
            for (size_t k = 0; k < gVec.size(); k++)
            {
                I += gVec[k].color * RayIntegral(k, tauVec);
            }

            uint8_t r = (uint8_t)(glm::clamp(I.x, 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(glm::clamp(I.y, 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(glm::clamp(I.z, 0.0f, 1.0f) * 255.0f);

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
