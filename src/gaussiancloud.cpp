#include "gaussiancloud.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "core/log.h"
#include "core/util.h"

static bool CheckLine(std::ifstream& plyFile, const std::string& validLine)
{
    std::string line;
    return std::getline(plyFile, line) && line == validLine;
}

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

    // validate start of header
    if (!CheckLine(plyFile, "ply") ||
        !CheckLine(plyFile, "format binary_little_endian 1.0"))
    {
        Log::E("Invalid ply file\n");
        return false;
    }

    // parse "element vertex 123"
    std::string line;
    if (!std::getline(plyFile, line))
    {
        Log::E("Invalid ply file\n");
        return false;
    }
    std::istringstream iss(line);
    std::string token1, token2;
    int numGaussians;
    if (!((iss >> token1 >> token2 >> numGaussians) && (token1 == "element") && (token2 == "vertex")))
    {
        Log::E("Invalid ply file\n");
        return false;
    }

    // validate rest of header
    if (!CheckLine(plyFile, "property float x") ||
        !CheckLine(plyFile, "property float y") ||
        !CheckLine(plyFile, "property float z") ||
        !CheckLine(plyFile, "property float nx") ||
        !CheckLine(plyFile, "property float ny") ||
        !CheckLine(plyFile, "property float nz") ||
        !CheckLine(plyFile, "property float f_dc_0") ||
        !CheckLine(plyFile, "property float f_dc_1") ||
        !CheckLine(plyFile, "property float f_dc_2") ||
        !CheckLine(plyFile, "property float f_rest_0") ||
        !CheckLine(plyFile, "property float f_rest_1") ||
        !CheckLine(plyFile, "property float f_rest_2") ||
        !CheckLine(plyFile, "property float f_rest_3") ||
        !CheckLine(plyFile, "property float f_rest_4") ||
        !CheckLine(plyFile, "property float f_rest_5") ||
        !CheckLine(plyFile, "property float f_rest_6") ||
        !CheckLine(plyFile, "property float f_rest_7") ||
        !CheckLine(plyFile, "property float f_rest_8") ||
        !CheckLine(plyFile, "property float f_rest_9") ||
        !CheckLine(plyFile, "property float f_rest_10") ||
        !CheckLine(plyFile, "property float f_rest_11") ||
        !CheckLine(plyFile, "property float f_rest_12") ||
        !CheckLine(plyFile, "property float f_rest_13") ||
        !CheckLine(plyFile, "property float f_rest_14") ||
        !CheckLine(plyFile, "property float f_rest_15") ||
        !CheckLine(plyFile, "property float f_rest_16") ||
        !CheckLine(plyFile, "property float f_rest_17") ||
        !CheckLine(plyFile, "property float f_rest_18") ||
        !CheckLine(plyFile, "property float f_rest_19") ||
        !CheckLine(plyFile, "property float f_rest_20") ||
        !CheckLine(plyFile, "property float f_rest_21") ||
        !CheckLine(plyFile, "property float f_rest_22") ||
        !CheckLine(plyFile, "property float f_rest_23") ||
        !CheckLine(plyFile, "property float f_rest_24") ||
        !CheckLine(plyFile, "property float f_rest_25") ||
        !CheckLine(plyFile, "property float f_rest_26") ||
        !CheckLine(plyFile, "property float f_rest_27") ||
        !CheckLine(plyFile, "property float f_rest_28") ||
        !CheckLine(plyFile, "property float f_rest_29") ||
        !CheckLine(plyFile, "property float f_rest_30") ||
        !CheckLine(plyFile, "property float f_rest_31") ||
        !CheckLine(plyFile, "property float f_rest_32") ||
        !CheckLine(plyFile, "property float f_rest_33") ||
        !CheckLine(plyFile, "property float f_rest_34") ||
        !CheckLine(plyFile, "property float f_rest_35") ||
        !CheckLine(plyFile, "property float f_rest_36") ||
        !CheckLine(plyFile, "property float f_rest_37") ||
        !CheckLine(plyFile, "property float f_rest_38") ||
        !CheckLine(plyFile, "property float f_rest_39") ||
        !CheckLine(plyFile, "property float f_rest_40") ||
        !CheckLine(plyFile, "property float f_rest_41") ||
        !CheckLine(plyFile, "property float f_rest_42") ||
        !CheckLine(plyFile, "property float f_rest_43") ||
        !CheckLine(plyFile, "property float f_rest_44") ||
        !CheckLine(plyFile, "property float opacity") ||
        !CheckLine(plyFile, "property float scale_0") ||
        !CheckLine(plyFile, "property float scale_1") ||
        !CheckLine(plyFile, "property float scale_2") ||
        !CheckLine(plyFile, "property float rot_0") ||
        !CheckLine(plyFile, "property float rot_1") ||
        !CheckLine(plyFile, "property float rot_2") ||
        !CheckLine(plyFile, "property float rot_3") ||
        !CheckLine(plyFile, "end_header"))
    {
        Log::E("Invalid ply file\n");
        return false;
    }

    // the data in the plyFile is packed
    // so we read it in one Gaussian structure at a time
    const size_t GAUSSIAN_SIZE = 62 * sizeof(float);
    static_assert(sizeof(Gaussian) >= GAUSSIAN_SIZE);

    glm::vec3 pos(-3.0089893469241797f, -0.11086489695181866f, -3.7527640949141428f);
    pos += glm::vec3(0.0f, 0.0f, 3.0f);
    const float SIZE = 300.0f;
    glm::vec3 aabbMin = pos + glm::vec3(-0.5f, -0.5f, -0.5f) * SIZE;
    glm::vec3 aabbMax = pos + glm::vec3(0.5f, 0.5f, 0.5f) * SIZE;

    gaussianVec.reserve(numGaussians);
    for (int i = 0; i < numGaussians; i++)
    {
        Gaussian g;
        plyFile.read((char*)&g, GAUSSIAN_SIZE);
        if (plyFile.gcount() != GAUSSIAN_SIZE)
        {
            Log::E("Error reading gaussian[%d]\n", i);
            return false;
        }
        glm::vec3 p(g.position[0], g.position[1], g.position[2]);
        if (PointInsideAABB(p, aabbMin, aabbMax))
        {
            gaussianVec.push_back(g);
        }
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
    const int NUM_SPLATS = 1;
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
    g.scale[0] = S * 0.5f; g.scale[1] = S; g.scale[2] = S;
    g.rot[0] = 1.0f; g.rot[1] = 0.0f; g.rot[2] = 0.0f; g.rot[3] = 0.0f;
    gaussianVec.push_back(g);
}
