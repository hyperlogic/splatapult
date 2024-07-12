/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "core/binaryattribute.h"

class PointCloud
{
public:
    PointCloud(bool useLinearColorsIn);

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    size_t GetNumPoints() const { return numPoints; }
    size_t GetStride() const { return pointSize; }
    size_t GetTotalSize() const { return GetNumPoints() * GetStride(); }
    void* GetRawDataPtr() { return data.get(); }
    const void* GetRawDataPtr() const { return data.get(); }

    const BinaryAttribute& GetPositionAttrib() const { return positionAttrib; }
    const BinaryAttribute& GetColorAttrib() const { return colorAttrib; }

    using ForEachPositionCallback = std::function<void(const float*)>;
    void ForEachPosition(const ForEachPositionCallback& cb) const;

protected:
    void InitAttribs();

    std::shared_ptr<void> data;

    BinaryAttribute positionAttrib;
    BinaryAttribute colorAttrib;

    size_t numPoints;
    size_t pointSize;
    bool useLinearColors;
};
