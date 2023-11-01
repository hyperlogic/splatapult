#include "pointcloud.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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
        std::cerr << "failed to open " << plyFilename << std::endl;
        return false;
    }

    // validate start of header
    if (!CheckLine(plyFile, "ply") ||
        !CheckLine(plyFile, "format binary_little_endian 1.0"))
    {
        std::cerr << "Invalid ply file" << std::endl;
        return false;
    }

    // parse "element vertex 123"
    std::string line;
    if (!GetNextPlyLine(plyFile, line))
    {
        std::cerr << "Invalid ply file, error reading element vertex count" << std::endl;
        return false;
    }
    std::istringstream iss(line);
    std::string token1, token2;
    int numPoints;
    if (!((iss >> token1 >> token2 >> numPoints) && (token1 == "element") && (token2 == "vertex")))
    {
        std::cerr << "Invalid ply file, error parsing element vertex count" << std::endl;
        return false;
    }

    // AJT: TODO support double properties
    // validate rest of header
    if (!CheckLine(plyFile, "property float x") ||
        !CheckLine(plyFile, "property float y") ||
        !CheckLine(plyFile, "property float z") ||
        !CheckLine(plyFile, "property float nx") ||
        !CheckLine(plyFile, "property float ny") ||
        !CheckLine(plyFile, "property float nz") ||
        !CheckLine(plyFile, "property uchar red") ||
        !CheckLine(plyFile, "property uchar green") ||
        !CheckLine(plyFile, "property uchar blue") ||
        !CheckLine(plyFile, "end_header"))
    {
        std::cerr << "Unsupported ply file, unexpected properties" << std::endl;
        return false;
    }

    // the data in the plyFile is packed
    // so we read it in one Point structure at a time
    const size_t POINT_SIZE = 27;
    static_assert(sizeof(Point) >= POINT_SIZE);

    pointVec.resize(numPoints);
    for (int i = 0; i < numPoints; i++)
    {
        plyFile.read((char*)&pointVec[i], POINT_SIZE);
        if (plyFile.gcount() != POINT_SIZE)
        {
            std::cerr << "Error reading point[" << i << "]" << std::endl;
            return false;
        }
    }

    return true;
}
