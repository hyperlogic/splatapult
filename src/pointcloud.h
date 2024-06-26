/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <string>
#include <vector>
#include <cstdint>

class PointCloud
{
public:
    PointCloud(bool useLinearColorsIn);

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    struct PointData
    {
        PointData() noexcept {}
        float position[4];
        float color[4];
    };

    size_t GetNumPoints() const { return pointDataVec.size(); }
    const std::vector<PointData>& GetPointDataVec() const { return pointDataVec; }
    std::vector<PointData>& GetPointDataVec() { return pointDataVec; }

protected:
    std::vector<PointData> pointDataVec;
    bool useLinearColors;
};
