/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class PointCloud
{
public:
    PointCloud(bool useLinearColorsIn);

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    struct AttribData
    {
        int32_t size;
        int32_t type;
        int32_t stride;
        size_t offset;
    };

    size_t GetNumPoints() const { return numPoints; }
    size_t GetTotalSize() const { return GetNumPoints() * pointSize; }
    void* GetRawDataPtr() { return data.get(); }

    const AttribData& GetPositionAttrib() const { return positionAttrib; }
    const AttribData& GetColorAttrib() const { return colorAttrib; }

    using AttribCallback = std::function<void(const void*)>;
    void ForEachAttrib(const AttribData& attribData, const AttribCallback& cb) const;

protected:
    std::shared_ptr<void> data;
    AttribData positionAttrib;
    AttribData colorAttrib;

    size_t numPoints;
    size_t pointSize;
    bool useLinearColors;
};
