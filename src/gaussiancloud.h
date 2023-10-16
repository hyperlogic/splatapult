#ifndef GAUSSIANCLOUD_H
#define GAUSSIANCLOUD_H

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

    struct Gaussian
    {
        float position[3];  // in world space
        float normal[3];  // unused
        float f_dc[3];  // first order spherical harmonics coeff
        float f_rest[45];  // more spherical harminics coeff
        float opacity;  // alpha = 1 / (1 + exp(-opacity));
        float scale[3];
        float rot[4];  // local rotation of guassian (real, i, j, k)

        // convert from (scale, rot) into the gaussian covariance matrix in world space
        // See 3d Gaussian Splat paper for more info
        glm::mat3 ComputeCovMat() const
        {
            glm::quat rot(rot[0], rot[1], rot[2], rot[3]);
            glm::mat3 R(glm::normalize(rot));
            glm::mat3 S(glm::vec3(expf(scale[0]), 0.0f, 0.0f),
                        glm::vec3(0.0f, expf(scale[1]), 0.0f),
                        glm::vec3(0.0f, 0.0f, expf(scale[2])));
            return R * S * glm::transpose(S) * glm::transpose(R);
        }
    };

    const std::vector<Gaussian>& GetGaussianVec() const { return gaussianVec; }
    std::vector<Gaussian>& GetGaussianVec() { return gaussianVec; }

protected:

    std::vector<Gaussian> gaussianVec;
};

#endif
