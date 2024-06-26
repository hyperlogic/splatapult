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
        Ply::PropertyInfo x, y, z;
        Ply::PropertyInfo red, green, blue;
    } props;

    if (!ply.GetPropertyInfo("x", props.x) ||
        !ply.GetPropertyInfo("y", props.y) ||
        !ply.GetPropertyInfo("z", props.z))
    {
        Log::E("Error parsing ply file \"%s\", missing position property\n", plyFilename.c_str());
    }

    bool useDoubles = (props.x.type == Ply::Type::Double && props.y.type == Ply::Type::Double && props.z.type == Ply::Type::Double);

    if (!ply.GetPropertyInfo("red", props.red) ||
        !ply.GetPropertyInfo("green", props.green) ||
        !ply.GetPropertyInfo("blue", props.blue))
    {
        Log::E("Error parsing ply file \"%s\", missing color property\n", plyFilename.c_str());
    }

    numPoints = ply.GetVertexCount();
    pointSize = sizeof(PointData);
    PointData* pointDataPtr = new PointData[numPoints];
    data.reset(pointDataPtr);

    // GL_FLOAT = 0x1406
    positionAttrib = {4, 0x1406, (int)pointSize, offsetof(PointData, position)};
    colorAttrib = {4, 0x1406, (int)pointSize, offsetof(PointData, color)};

    if (useDoubles)
    {
        int i = 0;
        ply.ForEachVertex([this, pointDataPtr, &i, &props](const uint8_t* data, size_t size)
        {
            if (useLinearColors)
            {
                pointDataPtr[i].position[0] = SRGBToLinear((float)props.x.Get<double>(data));
                pointDataPtr[i].position[1] = SRGBToLinear((float)props.y.Get<double>(data));
                pointDataPtr[i].position[2] = SRGBToLinear((float)props.z.Get<double>(data));
            }
            else
            {
                pointDataPtr[i].position[0] = (float)props.x.Get<double>(data);
                pointDataPtr[i].position[1] = (float)props.y.Get<double>(data);
                pointDataPtr[i].position[2] = (float)props.z.Get<double>(data);
            }
            pointDataPtr[i].position[3] = 1.0f;
            pointDataPtr[i].color[0] = (float)props.red.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[1] = (float)props.green.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[2] = (float)props.blue.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[3] = 1.0f;
            i++;
        });
    }
    else
    {
        int i = 0;
        ply.ForEachVertex([this, pointDataPtr, &i, &props](const uint8_t* data, size_t size)
        {
            if (useLinearColors)
            {
                pointDataPtr[i].position[0] = SRGBToLinear(props.x.Get<float>(data));
                pointDataPtr[i].position[1] = SRGBToLinear(props.y.Get<float>(data));
                pointDataPtr[i].position[2] = SRGBToLinear(props.z.Get<float>(data));
            }
            else
            {
                pointDataPtr[i].position[0] = props.x.Get<float>(data);
                pointDataPtr[i].position[1] = props.y.Get<float>(data);
                pointDataPtr[i].position[2] = props.z.Get<float>(data);
            }
            pointDataPtr[i].position[3] = 1.0f;
            pointDataPtr[i].color[0] = (float)props.red.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[1] = (float)props.green.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[2] = (float)props.blue.Get<uint8_t>(data) / 255.0f;
            pointDataPtr[i].color[3] = 1.0f;
            i++;
        });
    }

    return true;
}

bool PointCloud::ExportPly(const std::string& plyFilename) const
{
    // AJT: TODO FIXME BROKEN
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
    PointData* pointDataPtr = new PointData[numPoints];
    data.reset(pointDataPtr);

    //
    // make an debug pointVec, that contains three lines one for each axis.
    //
    const float AXIS_LENGTH = 1.0f;
    const float DELTA = (AXIS_LENGTH / (float)NUM_POINTS);
    // x axis
    for (int i = 0; i < NUM_POINTS; i++)
    {
        PointData& p = pointDataPtr[i];
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
        PointData& p = pointDataPtr[i + NUM_POINTS];
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
        PointData& p = pointDataPtr[i + 2 * NUM_POINTS];
        p.position[0] = 0.0f;
        p.position[1] = 0.0f;
        p.position[2] = i * DELTA;
        p.position[3] = 1.0f;
        p.color[0] = 0.0f;
        p.color[1] = 0.0f;
        p.color[2] = 1.0f;
        p.color[3] = 0.0f;
    }
}

void PointCloud::ForEachAttrib(const AttribData& attribData, const AttribCallback& cb) const
{
    const uint8_t* bytePtr = (uint8_t*)data.get();
    bytePtr += attribData.offset;
    for (size_t i = 0; i < GetNumPoints(); i++)
    {
        cb((const void*)bytePtr);
        bytePtr += attribData.stride;
    }
}
