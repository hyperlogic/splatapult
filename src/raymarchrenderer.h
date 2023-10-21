#ifndef RAYMARCHRENDERER_H
#define RAYMARCHRENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"

struct SDL_Renderer;

class RayMarchRenderer
{
public:
    RayMarchRenderer();
    ~RayMarchRenderer();

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::vec4& viewport,
                const glm::vec2& nearFar, float fovy, SDL_Renderer* renderer);
protected:
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

    std::vector<Gaussian> gVec;
};

#endif
