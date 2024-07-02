/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "gaussiancloud.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string.h>

#include <glm/gtc/quaternion.hpp>

#include <Eigen/Dense>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/log.h"
#include "core/util.h"

#include "ply.h"

struct BaseGaussianData
{
    BaseGaussianData() noexcept {}
    float posWithAlpha[4]; // center of the gaussian in object coordinates, with alpha in w
    float r_sh0[4]; // sh coeff for red channel (up to third-order)
    float g_sh0[4]; // sh coeff for green channel
    float b_sh0[4];  // sh coeff for blue channel
    float cov3_col0[3]; // 3x3 covariance matrix of the splat in object coordinates.
    float cov3_col1[3];
    float cov3_col2[3];
};

struct FullGaussianData : public BaseGaussianData
{
    FullGaussianData() noexcept {}
    float r_sh1[4];
    float r_sh2[4];
    float r_sh3[4];
    float g_sh1[4];
    float g_sh2[4];
    float g_sh3[4];
    float b_sh1[4];
    float b_sh2[4];
    float b_sh3[4];
};

// Function to convert glm::mat3 to Eigen::Matrix3f
static Eigen::Matrix3f glmToEigen(const glm::mat3& glmMat)
{
    Eigen::Matrix3f eigenMat;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            eigenMat(r, c) = glmMat[c][r];
        }
    }
    return eigenMat;
}

// Function to convert Eigen::Matrix3f to glm::mat3
static glm::mat3 eigenToGlm(const Eigen::Matrix3f& eigenMat)
{
    glm::mat3 glmMat;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            glmMat[c][r] = eigenMat(r, c);
        }
    }
    return glmMat;
}

static glm::mat3 ComputeCovMatFromRotScale(float rot[4], float scale[3])
{
    glm::quat q(rot[0], rot[1], rot[2], rot[3]);
    glm::mat3 R(glm::normalize(q));
    glm::mat3 S(glm::vec3(scale[0], 0.0f, 0.0f),
                glm::vec3(0.0f, scale[1], 0.0f),
                glm::vec3(0.0f, 0.0f, scale[2]));
    return R * S * glm::transpose(S) * glm::transpose(R);
}

static void ComputeRotScaleFromCovMat(const glm::mat3& V, glm::quat& rotOut, glm::vec3& scaleOut)
{
    Eigen::Matrix3f eigenV = glmToEigen(V);

    // Perform Eigen decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(eigenV);

    // Get eigenvectors and eigenvalues
    Eigen::Matrix3f eigenVec = solver.eigenvectors();
    Eigen::Array3f eigenVal = solver.eigenvalues();
    glm::mat3 R = eigenToGlm(eigenVec);
    // glm mat3 to quat only works when det is 1.
    if (glm::determinant(R) < 0)
    {
        R[0] *= -1.0f;
        R[1] *= -1.0f;
        R[2] *= -1.0f;
    }
    rotOut = glm::normalize(glm::quat(R));
    // The eigenVal gives us the diagonal of (S*S^T), so take the sqrt to give is S.
    scaleOut = glm::vec3(sqrtf(eigenVal(0)), sqrtf(eigenVal(1)), sqrtf(eigenVal(2)));
}

static float ComputeAlphaFromOpacity(float opacity)
{
    return 1.0f / (1.0f + expf(-opacity));
}

static float ComputeOpacityFromAlpha(float alpha)
{
    return -logf((1.0f / alpha) - 1.0f);
}

GaussianCloud::GaussianCloud(const Options& options) :
    numGaussians(0),
    gaussianSize(0),
    opt(options),
    hasFullSH(false)
{
    ;
}

bool GaussianCloud::ImportPly(const std::string& plyFilename)
{
    ZoneScopedNC("GC::ImportPly", tracy::Color::Red4);

    std::ifstream plyFile(plyFilename, std::ios::binary);
    if (!plyFile.is_open())
    {
        Log::E("failed to open %s\n", plyFilename.c_str());
        return false;
    }

    Ply ply;

    {
        ZoneScopedNC("ply.Parse", tracy::Color::Blue);
        if (!ply.Parse(plyFile))
        {
            Log::E("Error parsing ply file \"%s\"\n", plyFilename.c_str());
            return false;
        }
    }

    struct
    {
        BinaryAttribute x, y, z;
        BinaryAttribute f_dc[3];
        BinaryAttribute f_rest[45];
        BinaryAttribute opacity;
        BinaryAttribute scale[3];
        BinaryAttribute rot[4];
    } props;

    {
        ZoneScopedNC("ply.GetProps", tracy::Color::Green);

        if (!ply.GetProperty("x", props.x) ||
            !ply.GetProperty("y", props.y) ||
            !ply.GetProperty("z", props.z))
        {
            Log::E("Error parsing ply file \"%s\", missing position property\n", plyFilename.c_str());
        }

        for (int i = 0; i < 3; i++)
        {
            if (!ply.GetProperty("f_dc_" + std::to_string(i), props.f_dc[i]))
            {
                Log::E("Error parsing ply file \"%s\", missing f_dc property\n", plyFilename.c_str());
            }
        }

        if (opt.importFullSH)
        {
            hasFullSH = true;
            for (int i = 0; i < 45; i++)
            {
                if (!ply.GetProperty("f_rest_" + std::to_string(i), props.f_rest[i]))
                {
                    // f_rest properties are optional
                    Log::W("PLY file \"%s\", missing f_rest property\n", plyFilename.c_str());
                    hasFullSH = false;
                    break;
                }
            }
        }
        else
        {
            hasFullSH = false;
        }

        if (!ply.GetProperty("opacity", props.opacity))
        {
            Log::E("Error parsing ply file \"%s\", missing opacity property\n", plyFilename.c_str());
        }

        for (int i = 0; i < 3; i++)
        {
            if (!ply.GetProperty("scale_" + std::to_string(i), props.scale[i]))
            {
                Log::E("Error parsing ply file \"%s\", missing scale property\n", plyFilename.c_str());
            }
        }

        for (int i = 0; i < 4; i++)
        {
            if (!ply.GetProperty("rot_" + std::to_string(i), props.rot[i]))
            {
                Log::E("Error parsing ply file \"%s\", missing rot property\n", plyFilename.c_str());
            }
        }

    }

    InitAttribs();

    {
        ZoneScopedNC("alloc data", tracy::Color::Red4);

        numGaussians = ply.GetVertexCount();
        if (hasFullSH)
        {
            gaussianSize = sizeof(FullGaussianData);
            FullGaussianData* fullPtr = new FullGaussianData[numGaussians];
            data.reset(fullPtr);
        }
        else
        {
            gaussianSize = sizeof(BaseGaussianData);
            BaseGaussianData* basePtr = new BaseGaussianData[numGaussians];
            data.reset(basePtr);
        }
    }

    {
        ZoneScopedNC("ply.ForEachVertex", tracy::Color::Blue);
        int i = 0;
        uint8_t* rawPtr = (uint8_t*)data.get();
        ply.ForEachVertex([this, &rawPtr, &i, &props](const void* plyData, size_t size)
        {
            BaseGaussianData* basePtr = reinterpret_cast<BaseGaussianData*>(rawPtr);
            basePtr->posWithAlpha[0] = props.x.Read<float>(plyData);
            basePtr->posWithAlpha[1] = props.y.Read<float>(plyData);
            basePtr->posWithAlpha[2] = props.z.Read<float>(plyData);
            basePtr->posWithAlpha[3] = ComputeAlphaFromOpacity(props.opacity.Read<float>(plyData));

            if (hasFullSH)
            {
                FullGaussianData* fullPtr = reinterpret_cast<FullGaussianData*>(rawPtr);
                fullPtr->r_sh0[0] = props.f_dc[0].Read<float>(plyData);
                fullPtr->r_sh0[1] = props.f_rest[0].Read<float>(plyData);
                fullPtr->r_sh0[2] = props.f_rest[1].Read<float>(plyData);
                fullPtr->r_sh0[3] = props.f_rest[2].Read<float>(plyData);
                fullPtr->r_sh1[0] = props.f_rest[3].Read<float>(plyData);
                fullPtr->r_sh1[1] = props.f_rest[4].Read<float>(plyData);
                fullPtr->r_sh1[2] = props.f_rest[5].Read<float>(plyData);
                fullPtr->r_sh1[3] = props.f_rest[6].Read<float>(plyData);
                fullPtr->r_sh2[0] = props.f_rest[7].Read<float>(plyData);
                fullPtr->r_sh2[1] = props.f_rest[8].Read<float>(plyData);
                fullPtr->r_sh2[2] = props.f_rest[9].Read<float>(plyData);
                fullPtr->r_sh2[3] = props.f_rest[10].Read<float>(plyData);
                fullPtr->r_sh3[0] = props.f_rest[11].Read<float>(plyData);
                fullPtr->r_sh3[1] = props.f_rest[12].Read<float>(plyData);
                fullPtr->r_sh3[2] = props.f_rest[13].Read<float>(plyData);
                fullPtr->r_sh3[3] = props.f_rest[14].Read<float>(plyData);

                fullPtr->g_sh0[0] = props.f_dc[1].Read<float>(plyData);
                fullPtr->g_sh0[1] = props.f_rest[15].Read<float>(plyData);
                fullPtr->g_sh0[2] = props.f_rest[16].Read<float>(plyData);
                fullPtr->g_sh0[3] = props.f_rest[17].Read<float>(plyData);
                fullPtr->g_sh1[0] = props.f_rest[18].Read<float>(plyData);
                fullPtr->g_sh1[1] = props.f_rest[19].Read<float>(plyData);
                fullPtr->g_sh1[2] = props.f_rest[20].Read<float>(plyData);
                fullPtr->g_sh1[3] = props.f_rest[21].Read<float>(plyData);
                fullPtr->g_sh2[0] = props.f_rest[22].Read<float>(plyData);
                fullPtr->g_sh2[1] = props.f_rest[23].Read<float>(plyData);
                fullPtr->g_sh2[2] = props.f_rest[24].Read<float>(plyData);
                fullPtr->g_sh2[3] = props.f_rest[25].Read<float>(plyData);
                fullPtr->g_sh3[0] = props.f_rest[26].Read<float>(plyData);
                fullPtr->g_sh3[1] = props.f_rest[27].Read<float>(plyData);
                fullPtr->g_sh3[2] = props.f_rest[28].Read<float>(plyData);
                fullPtr->g_sh3[3] = props.f_rest[29].Read<float>(plyData);

                fullPtr->b_sh0[0] = props.f_dc[2].Read<float>(plyData);
                fullPtr->b_sh0[1] = props.f_rest[30].Read<float>(plyData);
                fullPtr->b_sh0[2] = props.f_rest[31].Read<float>(plyData);
                fullPtr->b_sh0[3] = props.f_rest[32].Read<float>(plyData);
                fullPtr->b_sh1[0] = props.f_rest[33].Read<float>(plyData);
                fullPtr->b_sh1[1] = props.f_rest[34].Read<float>(plyData);
                fullPtr->b_sh1[2] = props.f_rest[35].Read<float>(plyData);
                fullPtr->b_sh1[3] = props.f_rest[36].Read<float>(plyData);
                fullPtr->b_sh2[0] = props.f_rest[37].Read<float>(plyData);
                fullPtr->b_sh2[1] = props.f_rest[38].Read<float>(plyData);
                fullPtr->b_sh2[2] = props.f_rest[39].Read<float>(plyData);
                fullPtr->b_sh2[3] = props.f_rest[40].Read<float>(plyData);
                fullPtr->b_sh3[0] = props.f_rest[41].Read<float>(plyData);
                fullPtr->b_sh3[1] = props.f_rest[42].Read<float>(plyData);
                fullPtr->b_sh3[2] = props.f_rest[43].Read<float>(plyData);
                fullPtr->b_sh3[3] = props.f_rest[44].Read<float>(plyData);
            }
            else
            {
                basePtr->r_sh0[0] = props.f_dc[0].Read<float>(plyData);
                basePtr->r_sh0[1] = 0.0f;
                basePtr->r_sh0[2] = 0.0f;
                basePtr->r_sh0[3] = 0.0f;

                basePtr->g_sh0[0] = props.f_dc[1].Read<float>(plyData);
                basePtr->g_sh0[1] = 0.0f;
                basePtr->g_sh0[2] = 0.0f;
                basePtr->g_sh0[3] = 0.0f;

                basePtr->b_sh0[0] = props.f_dc[2].Read<float>(plyData);
                basePtr->b_sh0[1] = 0.0f;
                basePtr->b_sh0[2] = 0.0f;
                basePtr->b_sh0[3] = 0.0f;
            }

            // NOTE: scale is stored in logarithmic scale in plyFile
            float scale[3] =
            {
                expf(props.scale[0].Read<float>(plyData)),
                expf(props.scale[1].Read<float>(plyData)),
                expf(props.scale[2].Read<float>(plyData))
            };
            float rot[4] =
            {
                props.rot[0].Read<float>(plyData),
                props.rot[1].Read<float>(plyData),
                props.rot[2].Read<float>(plyData),
                props.rot[3].Read<float>(plyData)
            };

            glm::mat3 V = ComputeCovMatFromRotScale(rot, scale);
            basePtr->cov3_col0[0] = V[0][0];
            basePtr->cov3_col0[1] = V[0][1];
            basePtr->cov3_col0[2] = V[0][2];
            basePtr->cov3_col1[0] = V[1][0];
            basePtr->cov3_col1[1] = V[1][1];
            basePtr->cov3_col1[2] = V[1][2];
            basePtr->cov3_col2[0] = V[2][0];
            basePtr->cov3_col2[1] = V[2][1];
            basePtr->cov3_col2[2] = V[2][2];
            i++;
            rawPtr += gaussianSize;
        });
    }

    return true;
}

bool GaussianCloud::ExportPly(const std::string& plyFilename) const
{
    std::ofstream plyFile(plyFilename, std::ios::binary);
    if (!plyFile.is_open())
    {
        Log::E("failed to open %s\n", plyFilename.c_str());
        return false;
    }

    Ply ply;
    ply.AddProperty("x", BinaryAttribute::Type::Float);
    ply.AddProperty("y", BinaryAttribute::Type::Float);
    ply.AddProperty("z", BinaryAttribute::Type::Float);
    ply.AddProperty("nx", BinaryAttribute::Type::Float);
    ply.AddProperty("ny", BinaryAttribute::Type::Float);
    ply.AddProperty("nz", BinaryAttribute::Type::Float);
    ply.AddProperty("f_dc_0", BinaryAttribute::Type::Float);
    ply.AddProperty("f_dc_1", BinaryAttribute::Type::Float);
    ply.AddProperty("f_dc_2", BinaryAttribute::Type::Float);
    if (opt.exportFullSH)
    {
        for (int i = 0; i < 45; i++)
        {
            ply.AddProperty("f_rest_" + std::to_string(i), BinaryAttribute::Type::Float);
        }
    }
    ply.AddProperty("opacity", BinaryAttribute::Type::Float);
    ply.AddProperty("scale_0", BinaryAttribute::Type::Float);
    ply.AddProperty("scale_1", BinaryAttribute::Type::Float);
    ply.AddProperty("scale_2", BinaryAttribute::Type::Float);
    ply.AddProperty("rot_0", BinaryAttribute::Type::Float);
    ply.AddProperty("rot_1", BinaryAttribute::Type::Float);
    ply.AddProperty("rot_2", BinaryAttribute::Type::Float);
    ply.AddProperty("rot_3", BinaryAttribute::Type::Float);

    struct
    {
        BinaryAttribute x, y, z;
        BinaryAttribute nx, ny, nz;
        BinaryAttribute f_dc[3];
        BinaryAttribute f_rest[45];
        BinaryAttribute opacity;
        BinaryAttribute scale[3];
        BinaryAttribute rot[4];
    } props;

    ply.GetProperty("x", props.x);
    ply.GetProperty("y", props.y);
    ply.GetProperty("z", props.z);
    ply.GetProperty("nx", props.nx);
    ply.GetProperty("ny", props.ny);
    ply.GetProperty("nz", props.nz);
    ply.GetProperty("f_dc_0", props.f_dc[0]);
    ply.GetProperty("f_dc_1", props.f_dc[1]);
    ply.GetProperty("f_dc_2", props.f_dc[2]);
    if (opt.exportFullSH)
    {
        for (int i = 0; i < 45; i++)
        {
            ply.GetProperty("f_rest_" + std::to_string(i), props.f_rest[i]);
        }
    }
    ply.GetProperty("opacity", props.opacity);
    ply.GetProperty("scale_0", props.scale[0]);
    ply.GetProperty("scale_1", props.scale[1]);
    ply.GetProperty("scale_2", props.scale[2]);
    ply.GetProperty("rot_0", props.rot[0]);
    ply.GetProperty("rot_1", props.rot[1]);
    ply.GetProperty("rot_2", props.rot[2]);
    ply.GetProperty("rot_3", props.rot[3]);

    ply.AllocData(numGaussians);

    uint8_t* gData = (uint8_t*)data.get();
    size_t runningSize = 0;
    ply.ForEachVertexMut([this, &props, &gData, &runningSize](void* plyData, size_t size)
    {
        const float* posWithAlpha = posWithAlphaAttrib.Get<float>(gData);
        const float* r_sh0 = r_sh0Attrib.Get<float>(gData);
        const float* g_sh0 = g_sh0Attrib.Get<float>(gData);
        const float* b_sh0 = b_sh0Attrib.Get<float>(gData);
        const float* cov3_col0 = cov3_col0Attrib.Get<float>(gData);

        props.x.Write<float>(plyData, posWithAlpha[0]);
        props.y.Write<float>(plyData, posWithAlpha[1]);
        props.z.Write<float>(plyData, posWithAlpha[2]);
        props.nx.Write<float>(plyData, 0.0f);
        props.ny.Write<float>(plyData, 0.0f);
        props.nz.Write<float>(plyData, 0.0f);
        props.f_dc[0].Write<float>(plyData, r_sh0[0]);
        props.f_dc[1].Write<float>(plyData, g_sh0[0]);
        props.f_dc[2].Write<float>(plyData, b_sh0[0]);

        if (opt.exportFullSH)
        {
            // TODO: maybe just a raw memcopy would be faster
            for (int i = 0; i < 15; i++)
            {
                props.f_rest[i].Write<float>(plyData, r_sh0[i + 1]);
            }
            for (int i = 0; i < 15; i++)
            {
                props.f_rest[i + 15].Write<float>(plyData, g_sh0[i + 1]);
            }
            for (int i = 0; i < 15; i++)
            {
                props.f_rest[i + 30].Write<float>(plyData, b_sh0[i + 1]);
            }
        }

        props.opacity.Write<float>(plyData, ComputeOpacityFromAlpha(posWithAlpha[3]));

        glm::mat3 V(cov3_col0[0], cov3_col0[1], cov3_col0[2],
                    cov3_col0[3], cov3_col0[4], cov3_col0[5],
                    cov3_col0[6], cov3_col0[7], cov3_col0[8]);

        glm::quat rot;
        glm::vec3 scale;
        ComputeRotScaleFromCovMat(V, rot, scale);

        props.scale[0].Write<float>(plyData, logf(scale.x));
        props.scale[1].Write<float>(plyData, logf(scale.y));
        props.scale[2].Write<float>(plyData, logf(scale.z));
        props.rot[0].Write<float>(plyData, rot.w);
        props.rot[1].Write<float>(plyData, rot.x);
        props.rot[2].Write<float>(plyData, rot.y);
        props.rot[3].Write<float>(plyData, rot.z);

        gData += gaussianSize;
        runningSize += gaussianSize;
        assert(runningSize <= GetTotalSize());  // bad, we went off the end of the data ptr
    });

    ply.Dump(plyFile);

    return true;
}

void GaussianCloud::InitDebugCloud()
{
    const int NUM_SPLATS = 5;

    numGaussians = NUM_SPLATS * 3 + 1;
    gaussianSize = sizeof(FullGaussianData);
    InitAttribs();
    FullGaussianData* gd = new FullGaussianData[numGaussians];
    data.reset(gd);

    //
    // make an debug GaussianClound, that contain red, green and blue axes.
    //
    const float AXIS_LENGTH = 1.0f;
    const float DELTA = (AXIS_LENGTH / (float)NUM_SPLATS);
    const float COV_DIAG = 0.005f;
    const float SH_C0 = 0.28209479177387814f;
    const float SH_ONE = 1.0f / (2.0f * SH_C0);
    const float SH_ZERO = -1.0f / (2.0f * SH_C0);

    // x axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        FullGaussianData g;
        memset(&g, 0, sizeof(FullGaussianData));
        g.posWithAlpha[0] = i * DELTA + DELTA;
        g.posWithAlpha[1] = 0.0f;
        g.posWithAlpha[2] = 0.0f;
        g.posWithAlpha[3] = 1.0f;
        // red
        g.r_sh0[0] = SH_ONE; g.g_sh0[0] = SH_ZERO; g.b_sh0[0] = SH_ZERO;
        g.cov3_col0[0] = COV_DIAG; g.cov3_col1[1] = COV_DIAG; g.cov3_col2[2] = COV_DIAG;
        gd[i] = g;
    }
    // y axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        FullGaussianData g;
        memset(&g, 0, sizeof(FullGaussianData));
        g.posWithAlpha[0] = 0.0f;
        g.posWithAlpha[1] = i * DELTA + DELTA;
        g.posWithAlpha[2] = 0.0f;
        g.posWithAlpha[3] = 1.0f;
        // green
        g.r_sh0[0] = SH_ZERO; g.g_sh0[0] = SH_ONE; g.b_sh0[0] = SH_ZERO;
        g.cov3_col0[0] = COV_DIAG; g.cov3_col1[1] = COV_DIAG; g.cov3_col2[2] = COV_DIAG;
        gd[NUM_SPLATS + i] = g;
    }
    // z axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        FullGaussianData g;
        memset(&g, 0, sizeof(FullGaussianData));
        g.posWithAlpha[0] = 0.0f;
        g.posWithAlpha[1] = 0.0f;
        g.posWithAlpha[2] = i * DELTA + DELTA + 0.0001f; // AJT: HACK prevent div by zero for debug-shaders
        g.posWithAlpha[3] = 1.0f;
        // blue
        g.r_sh0[0] = SH_ZERO; g.g_sh0[0] = SH_ZERO; g.b_sh0[0] = SH_ONE;
        g.cov3_col0[0] = COV_DIAG; g.cov3_col1[1] = COV_DIAG; g.cov3_col2[2] = COV_DIAG;
        gd[(NUM_SPLATS * 2) + i] = g;
    }

    FullGaussianData g;
    memset(&g, 0, sizeof(FullGaussianData));
    g.posWithAlpha[0] = 0.0f;
    g.posWithAlpha[1] = 0.0f;
    g.posWithAlpha[2] = 0.0f;
    g.posWithAlpha[3] = 1.0f;
    // white
    g.r_sh0[0] = SH_ONE; g.g_sh0[0] = SH_ONE; g.b_sh0[0] = SH_ONE;
    g.cov3_col0[0] = COV_DIAG; g.cov3_col1[1] = COV_DIAG; g.cov3_col2[2] = COV_DIAG;
    gd[(NUM_SPLATS * 3)] = g;
}

// only keep the nearest splats
void GaussianCloud::PruneSplats(const glm::vec3& origin, uint32_t numSplats)
{
    if (!data || static_cast<size_t>(numSplats) >= numGaussians)
    {
        return;
    }

    using IndexDistPair = std::pair<uint32_t, float>;
    std::vector<IndexDistPair> indexDistVec;
    indexDistVec.reserve(numGaussians);
    uint8_t* rawPtr = (uint8_t*)data.get();
    for (uint32_t i = 0; i < numGaussians; i++)
    {
        BaseGaussianData* basePtr = reinterpret_cast<BaseGaussianData*>(rawPtr);
        glm::vec3 pos(basePtr->posWithAlpha[0], basePtr->posWithAlpha[1], basePtr->posWithAlpha[2]);
        indexDistVec.push_back(IndexDistPair(i, glm::distance(origin, pos)));
        rawPtr += gaussianSize;
    }

    std::sort(indexDistVec.begin(), indexDistVec.end(), [](const IndexDistPair& a, const IndexDistPair& b)
    {
        return a.second < b.second;
    });

    uint8_t* newData;
    if (hasFullSH)
    {
        FullGaussianData* fullPtr = new FullGaussianData[numSplats];
        newData = (uint8_t*)fullPtr;
    }
    else
    {
        BaseGaussianData* basePtr = new BaseGaussianData[numSplats];
        newData = (uint8_t*)basePtr;
    }
    rawPtr = (uint8_t*)data.get();
    uint8_t* rawPtr2 = newData;

    for (uint32_t i = 0; i < numSplats; i++)
    {
        memcpy(rawPtr2, rawPtr + indexDistVec[i].first * gaussianSize, gaussianSize);
        rawPtr2 += gaussianSize;
    }
    numGaussians = numSplats;
    data.reset(newData);
}

void GaussianCloud::ForEachPosWithAlpha(const ForEachPosWithAlphaCallback& cb) const
{
    posWithAlphaAttrib.ForEach<float>(GetRawDataPtr(), GetStride(), GetNumGaussians(), cb);
}

void GaussianCloud::InitAttribs()
{
    // BaseGaussianData attribs
    posWithAlphaAttrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, posWithAlpha)};
    r_sh0Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, r_sh0)};
    g_sh0Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, g_sh0)};
    b_sh0Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, b_sh0)};
    cov3_col0Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, cov3_col0)};
    cov3_col1Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, cov3_col1)};
    cov3_col2Attrib = {BinaryAttribute::Type::Float, offsetof(BaseGaussianData, cov3_col2)};

    // FullGaussianData attribs
    if (hasFullSH)
    {
        r_sh1Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, r_sh1)};
        r_sh2Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, r_sh2)};
        r_sh3Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, r_sh3)};
        g_sh1Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, g_sh1)};
        g_sh2Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, g_sh2)};
        g_sh3Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, g_sh3)};
        b_sh1Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, b_sh1)};
        b_sh2Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, b_sh2)};
        b_sh3Attrib = {BinaryAttribute::Type::Float, offsetof(FullGaussianData, b_sh3)};
    }
}
