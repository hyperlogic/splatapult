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

#include "core/binaryattribute.h"

class GaussianCloud
{
public:
    struct Options
    {
        bool importFullSH;
        bool exportFullSH;
    };

    GaussianCloud(const Options& options);

    bool ImportPly(const std::string& plyFilename);
    bool ExportPly(const std::string& plyFilename) const;

    void InitDebugCloud();

    // only keep the nearest splats
    void PruneSplats(const glm::vec3& origin, uint32_t numGaussians);

    size_t GetNumGaussians() const { return numGaussians; }
    size_t GetStride() const { return gaussianSize; }
    size_t GetTotalSize() const { return GetNumGaussians() * gaussianSize; }
    void* GetRawDataPtr() { return data.get(); }
    const void* GetRawDataPtr() const { return data.get(); }

    const BinaryAttribute& GetPosWithAlphaAttrib() const { return posWithAlphaAttrib; }
    const BinaryAttribute& GetR_SH0Attrib() const { return r_sh0Attrib; }
    const BinaryAttribute& GetR_SH1Attrib() const { return r_sh1Attrib; }
    const BinaryAttribute& GetR_SH2Attrib() const { return r_sh2Attrib; }
    const BinaryAttribute& GetR_SH3Attrib() const { return r_sh3Attrib; }
    const BinaryAttribute& GetG_SH0Attrib() const { return g_sh0Attrib; }
    const BinaryAttribute& GetG_SH1Attrib() const { return g_sh1Attrib; }
    const BinaryAttribute& GetG_SH2Attrib() const { return g_sh2Attrib; }
    const BinaryAttribute& GetG_SH3Attrib() const { return g_sh3Attrib; }
    const BinaryAttribute& GetB_SH0Attrib() const { return b_sh0Attrib; }
    const BinaryAttribute& GetB_SH1Attrib() const { return b_sh1Attrib; }
    const BinaryAttribute& GetB_SH2Attrib() const { return b_sh2Attrib; }
    const BinaryAttribute& GetB_SH3Attrib() const { return b_sh3Attrib; }
    const BinaryAttribute& GetCov3_Col0Attrib() const { return cov3_col0Attrib; }
    const BinaryAttribute& GetCov3_Col1Attrib() const { return cov3_col1Attrib; }
    const BinaryAttribute& GetCov3_Col2Attrib() const { return cov3_col2Attrib; }

    using ForEachPosWithAlphaCallback = std::function<void(const float*)>;
    void ForEachPosWithAlpha(const ForEachPosWithAlphaCallback& cb) const;

    bool HasFullSH() const { return hasFullSH; }

protected:
    void InitAttribs();

    std::shared_ptr<void> data;

    BinaryAttribute posWithAlphaAttrib;
    BinaryAttribute r_sh0Attrib;
    BinaryAttribute r_sh1Attrib;
    BinaryAttribute r_sh2Attrib;
    BinaryAttribute r_sh3Attrib;
    BinaryAttribute g_sh0Attrib;
    BinaryAttribute g_sh1Attrib;
    BinaryAttribute g_sh2Attrib;
    BinaryAttribute g_sh3Attrib;
    BinaryAttribute b_sh0Attrib;
    BinaryAttribute b_sh1Attrib;
    BinaryAttribute b_sh2Attrib;
    BinaryAttribute b_sh3Attrib;
    BinaryAttribute cov3_col0Attrib;
    BinaryAttribute cov3_col1Attrib;
    BinaryAttribute cov3_col2Attrib;

    size_t numGaussians;
    size_t gaussianSize;

    Options opt;
    bool hasFullSH;
};
