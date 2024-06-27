/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class GaussianCloud
{
public:
    GaussianCloud(bool useLinearColorsIn);

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    // only keep the nearest splats
    void PruneSplats(const glm::vec3& origin, uint32_t numGaussians);

    size_t GetNumGaussians() const { return numGaussians; }
    size_t GetTotalSize() const { return GetNumGaussians() * gaussianSize; }
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

    const AttribData& GetPosWithAlphaAttrib() const { return posWithAlphaAttrib; }
    const AttribData& GetR_SH0Attrib() const { return r_sh0Attrib; }
    const AttribData& GetR_SH1Attrib() const { return r_sh1Attrib; }
    const AttribData& GetR_SH2Attrib() const { return r_sh2Attrib; }
    const AttribData& GetR_SH3Attrib() const { return r_sh3Attrib; }
    const AttribData& GetG_SH0Attrib() const { return g_sh0Attrib; }
    const AttribData& GetG_SH1Attrib() const { return g_sh1Attrib; }
    const AttribData& GetG_SH2Attrib() const { return g_sh2Attrib; }
    const AttribData& GetG_SH3Attrib() const { return g_sh3Attrib; }
    const AttribData& GetB_SH0Attrib() const { return b_sh0Attrib; }
    const AttribData& GetB_SH1Attrib() const { return b_sh1Attrib; }
    const AttribData& GetB_SH2Attrib() const { return b_sh2Attrib; }
    const AttribData& GetB_SH3Attrib() const { return b_sh3Attrib; }
    const AttribData& GetCov3_Col0Attrib() const { return cov3_col0Attrib; }
    const AttribData& GetCov3_Col1Attrib() const { return cov3_col1Attrib; }
    const AttribData& GetCov3_Col2Attrib() const { return cov3_col2Attrib; }

    using AttribCallback = std::function<void(const void*)>;
    void ForEachAttrib(const AttribData& attribData, const AttribCallback& cb) const;

protected:
    void InitAttribs(size_t size);

    std::shared_ptr<void> data;

    AttribData posWithAlphaAttrib;
    AttribData r_sh0Attrib;
    AttribData r_sh1Attrib;
    AttribData r_sh2Attrib;
    AttribData r_sh3Attrib;
    AttribData g_sh0Attrib;
    AttribData g_sh1Attrib;
    AttribData g_sh2Attrib;
    AttribData g_sh3Attrib;
    AttribData b_sh0Attrib;
    AttribData b_sh1Attrib;
    AttribData b_sh2Attrib;
    AttribData b_sh3Attrib;
    AttribData cov3_col0Attrib;
    AttribData cov3_col1Attrib;
    AttribData cov3_col2Attrib;

    size_t numGaussians;
    size_t gaussianSize;
    bool useLinearColors;
};
