/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "pointcloud.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "core/log.h"
#include "ply.h"

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

    Ply ply;
    if (!ply.Parse(plyFile))
    {
        Log::E("Error parsing ply file \"%s\"\n", plyFilename.c_str());
        return false;
    }

    struct
    {
        Ply::Property x, y, z;
        Ply::Property red, green, blue;
    } props;

    if (!ply.GetProperty("x", props.x) || !ply.GetProperty("y", props.y) || !ply.GetProperty("z", props.z))
    {
        Log::E("Error parsing ply file \"%s\", missing position property\n", plyFilename.c_str());
    }

    bool useDoubles = (props.x.type == Ply::Type::Double && props.y.type == Ply::Type::Double && props.z.type == Ply::Type::Double);

    if (!ply.GetProperty("red", props.red) || !ply.GetProperty("green", props.green) || !ply.GetProperty("blue", props.blue))
    {
        Log::E("Error parsing ply file \"%s\", missing color property\n", plyFilename.c_str());
    }

    pointVec.resize(ply.GetVertexCount());

    if (useDoubles)
    {
        int i = 0;
        ply.ForEachVertex([this, &i, &props](const uint8_t* data, size_t size)
        {
            pointVec[i].position[0] = (float)props.x.Get<double>(data);
            pointVec[i].position[1] = (float)props.y.Get<double>(data);
            pointVec[i].position[2] = (float)props.z.Get<double>(data);
            pointVec[i].color[0] = props.red.Get<uint8_t>(data);
            pointVec[i].color[1] = props.green.Get<uint8_t>(data);
            pointVec[i].color[2] = props.blue.Get<uint8_t>(data);
            i++;
        });
    }
    else
    {
        int i = 0;
        ply.ForEachVertex([this, &i, &props](const uint8_t* data, size_t size)
        {
            pointVec[i].position[0] = props.x.Get<float>(data);
            pointVec[i].position[1] = props.y.Get<float>(data);
            pointVec[i].position[2] = props.z.Get<float>(data);
            pointVec[i].color[0] = props.red.Get<uint8_t>(data);
            pointVec[i].color[1] = props.green.Get<uint8_t>(data);
            pointVec[i].color[2] = props.blue.Get<uint8_t>(data);
            i++;
        });
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
