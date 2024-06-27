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

    size_t GetNumPoints() const { return numPoints; }
    size_t GetTotalSize() const { return GetNumPoints() * pointSize; }
    void* GetRawDataPtr() { return data.get(); }

    struct AttribData
    {
        AttribData() : size(0), type(0), stride(0), offset(0) {}
        AttribData(int32_t sizeIn, int32_t typeIn, int32_t strideIn, size_t offsetIn) : size(sizeIn), type(typeIn), stride(strideIn), offset(offsetIn) {}
        bool IsValid() const { return size != 0; }
        int32_t size;
        int32_t type;
        int32_t stride;
        size_t offset;
    };

    const AttribData& GetPositionAttrib() const { return positionAttrib; }
    const AttribData& GetColorAttrib() const { return colorAttrib; }

    using AttribCallback = std::function<void(const void*)>;
    void ForEachAttrib(const AttribData& attribData, const AttribCallback& cb) const;

protected:
    void InitAttribs(size_t size);

    std::shared_ptr<void> data;

    AttribData positionAttrib;
    AttribData colorAttrib;

    size_t numPoints;
    size_t pointSize;
    bool useLinearColors;
};
