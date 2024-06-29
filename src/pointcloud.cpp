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
#include "core/util.h"
#include "ply.h"

struct PointData
{
    PointData() noexcept {}
    float position[4];
    float color[4];
};

PointCloud::PointCloud(bool useLinearColorsIn) :
    numPoints(0),
    pointSize(0),
    useLinearColors(useLinearColorsIn)
{
    ;
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
        BinaryAttribute x, y, z;
        BinaryAttribute red, green, blue;
    } props;

    if (!ply.GetProperty("x", props.x) ||
        !ply.GetProperty("y", props.y) ||
        !ply.GetProperty("z", props.z))
    {
        Log::E("Error parsing ply file \"%s\", missing position property\n", plyFilename.c_str());
    }

    bool useDoubles = (props.x.type == BinaryAttribute::Type::Double &&
                       props.y.type == BinaryAttribute::Type::Double &&
                       props.z.type == BinaryAttribute::Type::Double);

    if (!ply.GetProperty("red", props.red) ||
        !ply.GetProperty("green", props.green) ||
        !ply.GetProperty("blue", props.blue))
    {
        Log::E("Error parsing ply file \"%s\", missing color property\n", plyFilename.c_str());
    }

    numPoints = ply.GetVertexCount();
    pointSize = sizeof(PointData);
    InitAttribs();
    PointData* pd = new PointData[numPoints];
    data.reset(pd);

    if (useDoubles)
    {
        int i = 0;
        ply.ForEachVertex([this, pd, &i, &props](const uint8_t* data, size_t size)
        {
            if (useLinearColors)
            {
                pd[i].position[0] = SRGBToLinear((float)props.x.Read<double>(data));
                pd[i].position[1] = SRGBToLinear((float)props.y.Read<double>(data));
                pd[i].position[2] = SRGBToLinear((float)props.z.Read<double>(data));
            }
            else
            {
                pd[i].position[0] = (float)props.x.Read<double>(data);
                pd[i].position[1] = (float)props.y.Read<double>(data);
                pd[i].position[2] = (float)props.z.Read<double>(data);
            }
            pd[i].position[3] = 1.0f;
            pd[i].color[0] = (float)props.red.Read<uint8_t>(data) / 255.0f;
            pd[i].color[1] = (float)props.green.Read<uint8_t>(data) / 255.0f;
            pd[i].color[2] = (float)props.blue.Read<uint8_t>(data) / 255.0f;
            pd[i].color[3] = 1.0f;
            i++;
        });
    }
    else
    {
        int i = 0;
        ply.ForEachVertex([this, pd, &i, &props](const uint8_t* data, size_t size)
        {
            if (useLinearColors)
            {
                pd[i].position[0] = SRGBToLinear(props.x.Read<float>(data));
                pd[i].position[1] = SRGBToLinear(props.y.Read<float>(data));
                pd[i].position[2] = SRGBToLinear(props.z.Read<float>(data));
            }
            else
            {
                pd[i].position[0] = props.x.Read<float>(data);
                pd[i].position[1] = props.y.Read<float>(data);
                pd[i].position[2] = props.z.Read<float>(data);
            }
            pd[i].position[3] = 1.0f;
            pd[i].color[0] = (float)props.red.Read<uint8_t>(data) / 255.0f;
            pd[i].color[1] = (float)props.green.Read<uint8_t>(data) / 255.0f;
            pd[i].color[2] = (float)props.blue.Read<uint8_t>(data) / 255.0f;
            pd[i].color[3] = 1.0f;
            i++;
        });
    }

    return true;
}

bool PointCloud::ExportPly(const std::string& plyFilename) const
{
    // AJT: TODO
    return false;

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
    ply.AddProperty("red", BinaryAttribute::Type::UChar);
    ply.AddProperty("green", BinaryAttribute::Type::UChar);
    ply.AddProperty("blue", BinaryAttribute::Type::UChar);

    ply.AllocData(numPoints);

    /*
    ply.ForEachVertex([this](uint8_t* data, size_t size))
    {
        positionAttrib.Set
    }
    */
    
    /*
    // ply files have unix line endings.
    plyFile << "ply\n";
    plyFile << "format binary_little_endian 1.0\n";
    plyFile << "element vertex " << numPoints << "\n";
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
    */

    /*
    const size_t POINT_SIZE = 27;
    for (auto&& p : pointDataVec)
    {
        plyFile.write((char*)&p, POINT_SIZE);
    }
    */

    return true;
}

void PointCloud::InitDebugCloud()
{
    const int NUM_POINTS = 5;

    numPoints = NUM_POINTS * 3;
    pointSize = sizeof(PointData);
    InitAttribs();
    PointData* pd = new PointData[numPoints];
    data.reset(pd);

    //
    // make an debug pointVec, that contains three lines one for each axis.
    //
    const float AXIS_LENGTH = 1.0f;
    const float DELTA = (AXIS_LENGTH / (float)NUM_POINTS);
    // x axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointData& p = pd[i];
        p.position[0] = i * DELTA;
        p.position[1] = 0.0f;
        p.position[2] = 0.0f;
        p.position[3] = 1.0f;
        p.color[0] = 1.0f;
        p.color[1] = 0.0f;
        p.color[2] = 0.0f;
        p.color[3] = 1.0f;
    }
    // y axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointData& p = pd[i + NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = i * DELTA;
        p.position[2] = 0.0f;
        p.position[3] = 1.0f;
        p.color[0] = 0.0f;
        p.color[1] = 1.0f;
        p.color[2] = 0.0f;
        p.color[3] = 1.0f;
    }
    // z axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointData& p = pd[(2 * NUM_POINTS) + i];
        p.position[0] = 0.0f;
        p.position[1] = 0.0f;
        p.position[2] = i * DELTA;
        p.position[3] = 1.0f;
        p.color[0] = 0.0f;
        p.color[1] = 0.0f;
        p.color[2] = 1.0f;
        p.color[3] = 1.0f;
    }
}

void PointCloud::ForEachPosition(const ForEachPositionCallback& cb) const
{
    positionAttrib.ForEachConst<float>(GetRawDataPtr(), GetStride(), GetNumPoints(), cb);
}

void PointCloud::InitAttribs()
{
    positionAttrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(PointData, position)};
    colorAttrib = {BinaryAttribute::Type::Float, sizeof(float), offsetof(PointData, color)};
}
