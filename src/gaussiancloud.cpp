#include "gaussiancloud.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "core/log.h"
#include "core/util.h"

#include "ply.h"

GaussianCloud::GaussianCloud()
{
}

bool GaussianCloud::ImportPly(const std::string& plyFilename)
{
    std::ifstream plyFile(plyFilename, std::ios::binary);
    if (!plyFile.is_open())
    {
        Log::E("failed to open %s\n", plyFilename.c_str());
        return false;
    }

    Ply ply;
    if (!ply.Parse(plyFile))
    {
        Log::E("Error parsing ply file \"%s\"\n", plyFilename.c_str());
        return false;
    }

    struct
    {
        Ply::Property x, y, z;
        Ply::Property f_dc[3];
        Ply::Property f_rest[45];
        Ply::Property opacity;
        Ply::Property scale[3];
        Ply::Property rot[4];
    } props;

    if (!ply.GetProperty("x", props.x) || !ply.GetProperty("y", props.y) || !ply.GetProperty("z", props.z))
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
            Log::E("Error parsing ply file \"%s\", missing f_rest property\n", plyFilename.c_str());
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

    gaussianVec.resize(ply.GetVertexCount());

    int i = 0;
    ply.ForEachVertex([this, &i, &props](const uint8_t* data, size_t size)
    {
        gaussianVec[i].position[0] = props.x.Get<float>(data);
        gaussianVec[i].position[1] = props.y.Get<float>(data);
        gaussianVec[i].position[2] = props.z.Get<float>(data);
        for (int j = 0; j < 3; j++)
        {
            gaussianVec[i].f_dc[j] = props.f_dc[j].Get<float>(data);
        }
        for (int j = 0; j < 45; j++)
        {
            gaussianVec[i].f_rest[j] = props.f_rest[j].Get<float>(data);
        }
        gaussianVec[i].opacity = props.opacity.Get<float>(data);
        for (int j = 0; j < 3; j++)
        {
            gaussianVec[i].scale[j] = props.scale[j].Get<float>(data);
        }
        for (int j = 0; j < 4; j++)
        {
            gaussianVec[i].rot[j] = props.rot[j].Get<float>(data);
        }
        i++;
    });

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

    // ply files have unix line endings.
    plyFile << "ply\n";
    plyFile << "format binary_little_endian 1.0\n";
    plyFile << "element vertex " << gaussianVec.size() << "\n";
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

    const size_t GAUSSIAN_SIZE = 62 * sizeof(float);
    static_assert(sizeof(Gaussian) >= GAUSSIAN_SIZE);

    for (auto&& g : gaussianVec)
    {
        plyFile.write((char*)&g, GAUSSIAN_SIZE);
    }

    return true;
}

void GaussianCloud::InitDebugCloud()
{
    gaussianVec.clear();

    //
    // make an debug GaussianClound, that contain red, green and blue axes.
    //
    const float AXIS_LENGTH = 1.0f;
    const int NUM_SPLATS = 5;
    const float DELTA = (AXIS_LENGTH / (float)NUM_SPLATS);
    const float S = logf(0.05f);
    const float SH_C0 = 0.28209479177387814f;
    const float SH_ONE = 1.0f / (2.0f * SH_C0);
    const float SH_ZERO = -1.0f / (2.0f * SH_C0);

    // x axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = i * DELTA + DELTA;
        g.position[1] = 0.0f;
        g.position[2] = 0.0f;
        // red
        g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // y axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = i * DELTA + DELTA;
        g.position[2] = 0.0f;
        // green
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ZERO;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }
    // z axis
    for (int i = 0; i < NUM_SPLATS; i++)
    {
        GaussianCloud::Gaussian g;
        memset(&g, 0, sizeof(GaussianCloud::Gaussian));
        g.position[0] = 0.0f;
        g.position[1] = 0.0f;
        g.position[2] = i * DELTA + DELTA + 0.0001f; // AJT: HACK prevent div by zero for debug-shaders
        // blue
        g.f_dc[0] = SH_ZERO; g.f_dc[1] = SH_ZERO; g.f_dc[2] = SH_ONE;
        g.opacity = 100.0f;
        g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
        g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
        gaussianVec.push_back(g);
    }

    GaussianCloud::Gaussian g;
    memset(&g, 0, sizeof(GaussianCloud::Gaussian));
    g.position[0] = 0.0f;
    g.position[1] = 0.0f;
    g.position[2] = 0.0f;
    // white
    g.f_dc[0] = SH_ONE; g.f_dc[1] = SH_ONE; g.f_dc[2] = SH_ONE;
    g.opacity = 100.0f;
    g.scale[0] = S; g.scale[1] = S; g.scale[2] = S;
    g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    gaussianVec.push_back(g);
}

// only keep the nearest splats
void GaussianCloud::PruneSplats(const glm::vec3& origin, uint32_t numSplats)
{
    using IndexDistPair = std::pair<uint32_t, float>;
    std::vector<IndexDistPair> indexDistVec;
    indexDistVec.reserve(gaussianVec.size());
    for (uint32_t i = 0; i < gaussianVec.size(); i++)
    {
        glm::vec3 pos(gaussianVec[i].position[0], gaussianVec[i].position[1], gaussianVec[i].position[2]);
        indexDistVec.push_back(IndexDistPair(i, glm::distance(origin, pos)));
    }

    std::sort(indexDistVec.begin(), indexDistVec.end(), [](const IndexDistPair& a, const IndexDistPair& b)
    {
        return a.second < b.second;
    });

    std::vector<Gaussian> newGaussianVec;
    newGaussianVec.reserve(numSplats);
    for (uint32_t i = 0; i < numSplats; i++)
    {
        newGaussianVec.push_back(gaussianVec[indexDistVec[i].first]);
    }

    gaussianVec.swap(newGaussianVec);
}
