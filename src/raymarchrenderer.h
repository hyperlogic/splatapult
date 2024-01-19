/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"
#include "softwarerenderer.h"

struct SDL_Renderer;

class RayMarchRenderer : public SoftwareRenderer
{
public:
    RayMarchRenderer(SDL_Renderer* sdlRendererIn);
    virtual ~RayMarchRenderer();

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud);

    // viewport = (x, y, width, height)
    virtual void RenderImpl(const glm::mat4& cameraMat, const glm::mat4& projMat,
							const glm::vec4& viewport, const glm::vec2& nearFar) override;

    struct Gaussian
    {
        Gaussian() {}
        Gaussian(const glm::vec3& pIn, const glm::mat3& covIn, const glm::vec3& colorIn, float alphaIn) :
            p(pIn), cov(covIn), color(colorIn), alpha(alphaIn)
        {
            // Uhh, I'm trying to mimic the same normalization coefficient of the guassian splatting renderer.
            // I *think* this cancels out the 3d/2d projection.
            k = 6.28319f;  // 2 Pi
            covInv = glm::inverse(cov);
        }

        float Eval(const glm::vec3& x) const
        {
            glm::vec3 d = x - p;
            return k * expf(-0.5f * glm::dot(d, covInv * d));
        }

        glm::vec3 p;
        glm::mat3 cov;
        glm::vec3 color;
        float alpha;

        float k;
        glm::mat3 covInv;
    };
protected:
    std::vector<Gaussian> gVec;
};
