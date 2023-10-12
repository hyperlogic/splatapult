#ifndef GAUSSIANCLOUD_H
#define GAUSSIANCLOUD_H

#include <string>
#include <vector>

class GaussianCloud
{
public:
    GaussianCloud();

    bool ImportPly(const std::string& plyFilename);

    struct Gaussian
    {
        float position[3];
        float normal[3];
        float f_dc[3];
        float f_rest[45];
        float opacity;
        float scale[3];
        float rot[4];
    };

    const std::vector<Gaussian>& GetGaussianVec() const { return gaussianVec; }

    // AJT: HACK FOR NOW give full access to gaussianVec
    std::vector<Gaussian>& GetGaussianVec() { return gaussianVec; }

protected:

    std::vector<Gaussian> gaussianVec;
};

#endif
