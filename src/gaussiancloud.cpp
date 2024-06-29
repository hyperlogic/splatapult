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

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/log.h"
#include "core/util.h"

#include "ply.h"

struct GaussianData
{
    GaussianData() noexcept {}
    float posWithAlpha[4]; // center of the gaussian in object coordinates, with alpha in w
    float r_sh0[4]; // sh coeff for red channel (up to third-order)
    float r_sh1[4];
    float r_sh2[4];
    float r_sh3[4];
    float g_sh0[4]; // sh coeff for green channel
    float g_sh1[4];
    float g_sh2[4];
    float g_sh3[4];
    float b_sh0[4];  // sh coeff for blue channel
    float b_sh1[4];
    float b_sh2[4];
    float b_sh3[4];
    float cov3_col0[3]; // 3x3 covariance matrix of the splat in object coordinates.
    float cov3_col1[3];
    float cov3_col2[3];
};

glm::mat3 ComputeCovMat(float rot[4], float scale[3])
{
    glm::quat q(rot[0], rot[1], rot[2], rot[3]);
    glm::mat3 R(glm::normalize(q));
    glm::mat3 S(glm::vec3(expf(scale[0]), 0.0f, 0.0f),
                glm::vec3(0.0f, expf(scale[1]), 0.0f),
                glm::vec3(0.0f, 0.0f, expf(scale[2])));
    return R * S * glm::transpose(S) * glm::transpose(R);
}

float ComputeAlpha(float opacity)
{
    return 1.0f / (1.0f + expf(-opacity));
}

GaussianCloud::GaussianCloud(bool useLinearColorsIn) :
    numGaussians(0),
    gaussianSize(0),
    useLinearColors(useLinearColorsIn)
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

        for (int i = 0; i < 45; i++)
        {
            if (!ply.GetProperty("f_rest_" + std::to_string(i), props.f_rest[i]))
            {
                // f_rest properties are optional
                Log::W("PLY file \"%s\", missing f_rest property\n", plyFilename.c_str());
                break;
            }
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

    GaussianData* gd;
    {
        ZoneScopedNC("alloc data", tracy::Color::Red4);

        // AJT: TODO support with and without sh.

        numGaussians = ply.GetVertexCount();
        gaussianSize = sizeof(GaussianData);
        InitAttribs();
        gd = new GaussianData[numGaussians];
        data.reset(gd);
    }

    {
        ZoneScopedNC("ply.ForEachVertex", tracy::Color::Blue);
        int i = 0;
        ply.ForEachVertex([this, gd, &i, &props](const uint8_t* data, size_t size)
        {
            gd[i].posWithAlpha[0] = props.x.Read<float>(data);
            gd[i].posWithAlpha[1] = props.y.Read<float>(data);
            gd[i].posWithAlpha[2] = props.z.Read<float>(data);
            gd[i].posWithAlpha[3] = ComputeAlpha(props.opacity.Read<float>(data));

            // AJT: TODO check for useLinearColors

            gd[i].r_sh0[0] = props.f_dc[0].Read<float>(data);
            gd[i].r_sh0[1] = props.f_rest[0].Read<float>(data);
            gd[i].r_sh0[2] = props.f_rest[1].Read<float>(data);
            gd[i].r_sh0[3] = props.f_rest[2].Read<float>(data);
            gd[i].r_sh1[0] = props.f_rest[3].Read<float>(data);
            gd[i].r_sh1[1] = props.f_rest[4].Read<float>(data);
            gd[i].r_sh1[2] = props.f_rest[5].Read<float>(data);
            gd[i].r_sh1[3] = props.f_rest[6].Read<float>(data);
            gd[i].r_sh2[0] = props.f_rest[7].Read<float>(data);
            gd[i].r_sh2[1] = props.f_rest[8].Read<float>(data);
            gd[i].r_sh2[2] = props.f_rest[9].Read<float>(data);
            gd[i].r_sh2[3] = props.f_rest[10].Read<float>(data);
            gd[i].r_sh3[0] = props.f_rest[11].Read<float>(data);
            gd[i].r_sh3[1] = props.f_rest[12].Read<float>(data);
            gd[i].r_sh3[2] = props.f_rest[13].Read<float>(data);
            gd[i].r_sh3[3] = props.f_rest[14].Read<float>(data);

            gd[i].g_sh0[0] = props.f_dc[1].Read<float>(data);
            gd[i].g_sh0[1] = props.f_rest[15].Read<float>(data);
            gd[i].g_sh0[2] = props.f_rest[16].Read<float>(data);
            gd[i].g_sh0[3] = props.f_rest[17].Read<float>(data);
            gd[i].g_sh1[0] = props.f_rest[18].Read<float>(data);
            gd[i].g_sh1[1] = props.f_rest[19].Read<float>(data);
            gd[i].g_sh1[2] = props.f_rest[20].Read<float>(data);
            gd[i].g_sh1[3] = props.f_rest[21].Read<float>(data);
            gd[i].g_sh2[0] = props.f_rest[22].Read<float>(data);
            gd[i].g_sh2[1] = props.f_rest[23].Read<float>(data);
            gd[i].g_sh2[2] = props.f_rest[24].Read<float>(data);
            gd[i].g_sh2[3] = props.f_rest[25].Read<float>(data);
            gd[i].g_sh3[0] = props.f_rest[26].Read<float>(data);
            gd[i].g_sh3[1] = props.f_rest[27].Read<float>(data);
            gd[i].g_sh3[2] = props.f_rest[28].Read<float>(data);
            gd[i].g_sh3[3] = props.f_rest[29].Read<float>(data);

            gd[i].b_sh0[0] = props.f_dc[2].Read<float>(data);
            gd[i].b_sh0[1] = props.f_rest[30].Read<float>(data);
            gd[i].b_sh0[2] = props.f_rest[31].Read<float>(data);
            gd[i].b_sh0[3] = props.f_rest[32].Read<float>(data);
            gd[i].b_sh1[0] = props.f_rest[33].Read<float>(data);
            gd[i].b_sh1[1] = props.f_rest[34].Read<float>(data);
            gd[i].b_sh1[2] = props.f_rest[35].Read<float>(data);
            gd[i].b_sh1[3] = props.f_rest[36].Read<float>(data);
            gd[i].b_sh2[0] = props.f_rest[37].Read<float>(data);
            gd[i].b_sh2[1] = props.f_rest[38].Read<float>(data);
            gd[i].b_sh2[2] = props.f_rest[39].Read<float>(data);
            gd[i].b_sh2[3] = props.f_rest[40].Read<float>(data);
            gd[i].b_sh3[0] = props.f_rest[41].Read<float>(data);
            gd[i].b_sh3[1] = props.f_rest[42].Read<float>(data);
            gd[i].b_sh3[2] = props.f_rest[43].Read<float>(data);
            gd[i].b_sh3[3] = props.f_rest[44].Read<float>(data);

            float rot[4] =
            {
                props.rot[0].Read<float>(data),
                props.rot[1].Read<float>(data),
                props.rot[2].Read<float>(data),
                props.rot[3].Read<float>(data)
            };
            float scale[4] =
            {
                props.scale[0].Read<float>(data),
                props.scale[1].Read<float>(data),
                props.scale[2].Read<float>(data)
            };

            glm::mat3 V = ComputeCovMat(rot, scale);
            gd[i].cov3_col0[0] = V[0][0];
            gd[i].cov3_col0[1] = V[0][1];
            gd[i].cov3_col0[2] = V[0][2];
            gd[i].cov3_col1[0] = V[1][0];
            gd[i].cov3_col1[1] = V[1][1];
            gd[i].cov3_col1[2] = V[1][2];
            gd[i].cov3_col2[0] = V[2][0];
            gd[i].cov3_col2[1] = V[2][1];
            gd[i].cov3_col2[2] = V[2][2];

            i++;
        });
    }

    return true;
}

bool GaussianCloud::ExportPly(const std::string& plyFilename) const
{
    // AJT: TODO FIXME:
    return false;

    std::ofstream plyFile(plyFilename, std::ios::binary);
    if (!plyFile.is_open())
    {
        Log::E("failed to open %s\n", plyFilename.c_str());
        return false;
    }

    // ply files have unix line endings.
    plyFile << "ply\n";
    plyFile << "format binary_little_endian 1.0\n";
    plyFile << "element vertex " << numGaussians << "\n";
    plyFile << "property float x\n";
    plyFile << "property float y\n";
    plyFile << "property float z\n";
    plyFile << "property float nx\n";
    plyFile << "property float ny\n";
    plyFile << "property float nz\n";
    plyFile << "property float f_dc_0\n";
    plyFile << "property float f_dc_1\n";
    plyFile << "property float f_dc_2\n";
    plyFile << "property float f_rest_0\n";
    plyFile << "property float f_rest_1\n";
    plyFile << "property float f_rest_2\n";
    plyFile << "property float f_rest_3\n";
    plyFile << "property float f_rest_4\n";
    plyFile << "property float f_rest_5\n";
    plyFile << "property float f_rest_6\n";
    plyFile << "property float f_rest_7\n";
    plyFile << "property float f_rest_8\n";
    plyFile << "property float f_rest_9\n";
    plyFile << "property float f_rest_10\n";
    plyFile << "property float f_rest_11\n";
    plyFile << "property float f_rest_12\n";
    plyFile << "property float f_rest_13\n";
    plyFile << "property float f_rest_14\n";
    plyFile << "property float f_rest_15\n";
    plyFile << "property float f_rest_16\n";
    plyFile << "property float f_rest_17\n";
    plyFile << "property float f_rest_18\n";
    plyFile << "property float f_rest_19\n";
    plyFile << "property float f_rest_20\n";
    plyFile << "property float f_rest_21\n";
    plyFile << "property float f_rest_22\n";
    plyFile << "property float f_rest_23\n";
    plyFile << "property float f_rest_24\n";
    plyFile << "property float f_rest_25\n";
    plyFile << "property float f_rest_26\n";
    plyFile << "property float f_rest_27\n";
    plyFile << "property float f_rest_28\n";
    plyFile << "property float f_rest_29\n";
    plyFile << "property float f_rest_30\n";
    plyFile << "property float f_rest_31\n";
    plyFile << "property float f_rest_32\n";
    plyFile << "property float f_rest_33\n";
    plyFile << "property float f_rest_34\n";
    plyFile << "property float f_rest_35\n";
    plyFile << "property float f_rest_36\n";
    plyFile << "property float f_rest_37\n";
    plyFile << "property float f_rest_38\n";
    plyFile << "property float f_rest_39\n";
    plyFile << "property float f_rest_40\n";
    plyFile << "property float f_rest_41\n";
    plyFile << "property float f_rest_42\n";
    plyFile << "property float f_rest_43\n";
    plyFile << "property float f_rest_44\n";
    plyFile << "property float opacity\n";
    plyFile << "property float scale_0\n";
    plyFile << "property float scale_1\n";
    plyFile << "property float scale_2\n";
    plyFile << "property float rot_0\n";
    plyFile << "property float rot_1\n";
    plyFile << "property float rot_2\n";
    plyFile << "property float rot_3\n";
    plyFile << "end_header\n";

    /* AJT: TODO FIXME
    const size_t GAUSSIAN_SIZE = 62 * sizeof(float);
    static_assert(sizeof(Gaussian) >= GAUSSIAN_SIZE);

    for (auto&& g : gaussianVec)
    {
        plyFile.write((char*)&g, GAUSSIAN_SIZE);
    }
    */

    return true;
}

void GaussianCloud::InitDebugCloud()
{
    const int NUM_SPLATS = 5;

    numGaussians = NUM_SPLATS * 3 + 1;
    gaussianSize = sizeof(GaussianData);
    InitAttribs();
    GaussianData* gd = new GaussianData[numGaussians];
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
        GaussianData g;
        memset(&g, 0, sizeof(GaussianData));
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
        GaussianData g;
        memset(&g, 0, sizeof(GaussianData));
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
        GaussianData g;
        memset(&g, 0, sizeof(GaussianData));
        g.posWithAlpha[0] = 0.0f;
        g.posWithAlpha[1] = 0.0f;
        g.posWithAlpha[2] = i * DELTA + DELTA + 0.0001f; // AJT: HACK prevent div by zero for debug-shaders
        g.posWithAlpha[3] = 1.0f;
        // blue
        g.r_sh0[0] = SH_ZERO; g.g_sh0[0] = SH_ZERO; g.b_sh0[0] = SH_ONE;
        g.cov3_col0[0] = COV_DIAG; g.cov3_col1[1] = COV_DIAG; g.cov3_col2[2] = COV_DIAG;
        gd[(NUM_SPLATS * 2) + i] = g;
    }

    GaussianData g;
    memset(&g, 0, sizeof(GaussianData));
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

    GaussianData* gd = (GaussianData*)data.get();
    using IndexDistPair = std::pair<uint32_t, float>;
    std::vector<IndexDistPair> indexDistVec;
    indexDistVec.reserve(numGaussians);
    for (uint32_t i = 0; i < numGaussians; i++)
    {
        glm::vec3 pos(gd[i].posWithAlpha[0], gd[i].posWithAlpha[1], gd[i].posWithAlpha[2]);
        indexDistVec.push_back(IndexDistPair(i, glm::distance(origin, pos)));
    }

    std::sort(indexDistVec.begin(), indexDistVec.end(), [](const IndexDistPair& a, const IndexDistPair& b)
    {
        return a.second < b.second;
    });

    GaussianData* gd2 = new GaussianData[numSplats];
    for (uint32_t i = 0; i < numSplats; i++)
    {
        gd2[i] = gd[indexDistVec[i].first];
    }
    numGaussians = numSplats;
    data.reset(gd2);
    gd = nullptr;
}

void GaussianCloud::ForEachPosWithAlpha(const ForEachPosWithAlphaCallback& cb) const
{
    posWithAlphaAttrib.ForEachConst<float>(GetRawDataPtr(), GetStride(), GetNumGaussians(), cb);
}

void GaussianCloud::InitAttribs()
{
    posWithAlphaAttrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, posWithAlpha)};
    r_sh0Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, r_sh0)};
    r_sh1Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, r_sh1)};
    r_sh2Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, r_sh2)};
    r_sh3Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, r_sh3)};
    g_sh0Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, g_sh0)};
    g_sh1Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, g_sh1)};
    g_sh2Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, g_sh2)};
    g_sh3Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, g_sh3)};
    b_sh0Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, b_sh0)};
    b_sh1Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, b_sh1)};
    b_sh2Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, b_sh2)};
    b_sh3Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, b_sh3)};
    cov3_col0Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, cov3_col0)};
    cov3_col1Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, cov3_col1)};
    cov3_col2Attrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(GaussianData, cov3_col2)};
}
