#include "pointcloud.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "core/log.h"

struct DoublePoint
{
    double position[3];
    double normal[3];
    uint8_t color[3];
};

static bool GetNextPlyLine(std::ifstream& plyFile, std::string& lineOut)
{
    while (std::getline(plyFile, lineOut))
    {
        // skip comment lines
        if (lineOut.find("comment", 0) != 0)
        {
            return true;
        }
    }
    return false;
}

static bool CheckLine(std::ifstream& plyFile, const std::string& validLine)
{
    std::string line;
    return GetNextPlyLine(plyFile, line) && line == validLine;
}

PointCloud::PointCloud()
{
}

bool PointCloud::ImportPly(const std::string& plyFilename)
{
    std::ifstream plyFile(plyFilename, std::ios::binary);
    if (!plyFile.is_open())
    {
        Log::E("failed to open \"%s\"\n", plyFilename.c_str());
        return false;
    }

    // validate start of header
    if (!CheckLine(plyFile, "ply") ||
        !CheckLine(plyFile, "format binary_little_endian 1.0"))
    {
        Log::E("Invalid ply file \"%s\"\n", plyFilename.c_str());
        return false;
    }

    // parse "element vertex 123"
    std::string line;
    if (!GetNextPlyLine(plyFile, line))
    {
        Log::E("Invalid ply file \"%s\", error reading element vertex count\n", plyFilename.c_str());
        return false;
    }
    std::istringstream iss(line);
    std::string token1, token2;
    int numPoints;
    if (!((iss >> token1 >> token2 >> numPoints) && (token1 == "element") && (token2 == "vertex")))
    {
        Log::E("Invalid ply file \"%s\", error parsing element vertex count\n", plyFilename.c_str());
        return false;
    }

    bool useDoubles = false;

    if (!GetNextPlyLine(plyFile, line))
    {
        Log::E("Invalid ply file \"%s\", error reading first property\n", plyFilename.c_str());
        return false;
    }

    if (line == "property double x")
    {
        useDoubles = true;
        // validate rest of header
        if (!CheckLine(plyFile, "property double y") ||
            !CheckLine(plyFile, "property double z") ||
            !CheckLine(plyFile, "property double nx") ||
            !CheckLine(plyFile, "property double ny") ||
            !CheckLine(plyFile, "property double nz") ||
            !CheckLine(plyFile, "property uchar red") ||
            !CheckLine(plyFile, "property uchar green") ||
            !CheckLine(plyFile, "property uchar blue") ||
            !CheckLine(plyFile, "end_header"))
        {
            Log::E("Unsupported ply file \"%s\", unexpected properties\n", plyFilename.c_str());
            return false;
        }
    }
    else if (line == "property float x")
    {
        useDoubles = false;
        // validate rest of header
        if (!CheckLine(plyFile, "property float y") ||
            !CheckLine(plyFile, "property float z") ||
            !CheckLine(plyFile, "property float nx") ||
            !CheckLine(plyFile, "property float ny") ||
            !CheckLine(plyFile, "property float nz") ||
            !CheckLine(plyFile, "property uchar red") ||
            !CheckLine(plyFile, "property uchar green") ||
            !CheckLine(plyFile, "property uchar blue") ||
            !CheckLine(plyFile, "end_header"))
        {
            Log::E("Unsupported ply file \"%s\", unexpected properties\n", plyFilename.c_str());
            return false;
        }
    }
    else
    {

        Log::E("Unsupported ply file \"%s\", unexpected first property\n", plyFilename.c_str());
        return false;
    }

    // the data in the plyFile is packed
    // so we read it in one Point structure at a time
    if (useDoubles)
    {
        const size_t POINT_SIZE = 51;
        assert(sizeof(DoublePoint) >= POINT_SIZE);
        pointVec.resize(numPoints);
        for (int i = 0; i < numPoints; i++)
        {
            DoublePoint dp;
            plyFile.read((char*)&dp, POINT_SIZE);
            if (plyFile.gcount() != POINT_SIZE)
            {
                Log::E("Error reading \"%s\", point[%d]\n", plyFilename.c_str(), i);
                return false;
            }
            // convert to float and copy into pointVec.
            pointVec[i].position[0] = (float)dp.position[0];
            pointVec[i].position[1] = (float)dp.position[1];
            pointVec[i].position[2] = (float)dp.position[2];
            pointVec[i].normal[0] = (float)dp.normal[0];
            pointVec[i].normal[1] = (float)dp.normal[1];
            pointVec[i].normal[2] = (float)dp.normal[2];
            pointVec[i].color[0] = dp.color[0];
            pointVec[i].color[1] = dp.color[1];
            pointVec[i].color[2] = dp.color[2];
        }
    }
    else
    {
        const size_t POINT_SIZE = 27;
        assert(sizeof(Point) >= POINT_SIZE);
        pointVec.resize(numPoints);
        for (int i = 0; i < numPoints; i++)
        {
            // read directly into Point structure
            plyFile.read((char*)&pointVec[i], POINT_SIZE);
            if (plyFile.gcount() != POINT_SIZE)
            {
                Log::E("Error reading \"%s\", point[%d]\n", plyFilename.c_str(), i);
                return false;
            }
        }
    }


    return true;
}

bool PointCloud::ExportPly(const std::string& plyFilename) const
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
    plyFile << "element vertex " << pointVec.size() << "\n";
    plyFile << "property float x\n";
    plyFile << "property float y\n";
    plyFile << "property float z\n";
    plyFile << "property float nx\n";
    plyFile << "property float ny\n";
    plyFile << "property float nz\n";
    plyFile << "property uchar red\n";
    plyFile << "property uchar green\n";
    plyFile << "property uchar blue\n";
    plyFile << "end_header\n";

    const size_t POINT_SIZE = 27;
    for (auto&& p : pointVec)
    {
        plyFile.write((char*)&p, POINT_SIZE);
    }

    return true;
}

void PointCloud::InitDebugCloud()
{
    pointVec.clear();

    //
    // make an debug pointVec, that contains three lines one for each axis.
    //
    const float AXIS_LENGTH = 1.0f;
    const int NUM_POINTS = 5;
    const float DELTA = (AXIS_LENGTH / (float)NUM_POINTS);
    pointVec.resize(NUM_POINTS * 3);
    // x axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i];
        p.position[0] = i * DELTA;
        p.position[1] = 0.0f;
        p.position[2] = 0.0f;
        p.color[0] = 255;
        p.color[1] = 0;
        p.color[2] = 0;
    }
    // y axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = i * DELTA;
        p.position[2] = 0.0f;
        p.color[0] = 0;
        p.color[1] = 255;
        p.color[2] = 0;
    }
    // z axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointCloud::Point& p = pointVec[i + 2 * NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = 0.0f;
        p.position[2] = i * DELTA;
        p.color[0] = 0;
        p.color[1] = 0;
        p.color[2] = 255;
    }
}
