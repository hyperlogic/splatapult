/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

class GaussianCloud
{
public:
    GaussianCloud();

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    // only keep the nearest splats
    void PruneSplats(const glm::vec3& origin, uint32_t numSplats);

    struct Gaussian
    {
        float position[3];  // in world space
        float normal[3];  // unused
        float f_dc[3];  // first order spherical harmonics coeff (sRGB color space)
        float f_rest[45];  // more spherical harminics coeff
        float opacity;  // alpha = 1 / (1 + exp(-opacity));
        float scale[3];
        float rot[4];  // local rotation of guassian (real, i, j, k)

        // convert from (scale, rot) into the gaussian covariance matrix in world space
        // See 3d Gaussian Splat paper for more info
        glm::mat3 ComputeCovMat() const
        {
            glm::quat q(rot[0], rot[1], rot[2], rot[3]);
            glm::mat3 R(glm::normalize(q));
            glm::mat3 S(glm::vec3(expf(scale[0]), 0.0f, 0.0f),
                        glm::vec3(0.0f, expf(scale[1]), 0.0f),
                        glm::vec3(0.0f, 0.0f, expf(scale[2])));
            return R * S * glm::transpose(S) * glm::transpose(R);
        }
    };

    const std::vector<Gaussian>& GetGaussianVec() const { return gaussianVec; }
    std::vector<Gaussian>& GetGaussianVec() { return gaussianVec; }
    size_t size() const { return gaussianVec.size(); }

protected:

    std::vector<Gaussian> gaussianVec;
};
