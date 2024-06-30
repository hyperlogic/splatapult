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
        ply.ForEachVertex([this, pd, &i, &props](const void* data, size_t size)
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
        ply.ForEachVertex([this, pd, &i, &props](const void* data, size_t size)
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

    struct
    {
        BinaryAttribute x, y, z;
        BinaryAttribute nx, ny, nz;
        BinaryAttribute red, green, blue;
    } props;

    ply.GetProperty("x", props.x);
    ply.GetProperty("y", props.y);
    ply.GetProperty("z", props.z);
    ply.GetProperty("nx", props.nx);
    ply.GetProperty("ny", props.ny);
    ply.GetProperty("nz", props.nz);
    ply.GetProperty("red", props.red);
    ply.GetProperty("green", props.green);
    ply.GetProperty("blue", props.blue);

    ply.AllocData(numPoints);

    uint8_t* cloudData = (uint8_t*)data.get();
    size_t runningSize = 0;
    ply.ForEachVertexMut([this, &props, &cloudData, &runningSize](void* plyData, size_t size)
    {
        const float* position = positionAttrib.Get<float>(cloudData);
        const float* color = colorAttrib.Get<float>(cloudData);

        props.x.Write<float>(plyData, position[0]);
        props.y.Write<float>(plyData, position[1]);
        props.z.Write<float>(plyData, position[2]);
        props.nx.Write<float>(plyData, 0.0f);
        props.ny.Write<float>(plyData, 0.0f);
        props.nz.Write<float>(plyData, 0.0f);
        props.red.Write<uint8_t>(plyData, (uint8_t)(color[0] * 255.0f));
        props.green.Write<uint8_t>(plyData, (uint8_t)(color[1] * 255.0f));
        props.blue.Write<uint8_t>(plyData, (uint8_t)(color[2] * 255.0f));

        cloudData += pointSize;
        runningSize += pointSize;
        assert(runningSize <= GetTotalSize());  // bad, we went outside of data ptr contents.
    });

    ply.Dump(plyFile);

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
    positionAttrib.ForEach<float>(GetRawDataPtr(), GetStride(), GetNumPoints(), cb);
}

void PointCloud::InitAttribs()
{
    positionAttrib = {BinaryAttribute::Type::Float, offsetof(PointData, position)};
    colorAttrib = {BinaryAttribute::Type::Float, offsetof(PointData, color)};
}
