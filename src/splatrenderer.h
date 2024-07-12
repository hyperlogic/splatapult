/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "core/program.h"
#include "core/vertexbuffer.h"

#include "gaussiancloud.h"

namespace rgc::radix_sort
{
    struct sorter;
}

class SplatRenderer
{
public:
    SplatRenderer();
    ~SplatRenderer();

    bool Init(std::shared_ptr<GaussianCloud> gaussianCloud,
              bool isFramebufferSRGBEnabledIn, bool useRgcSortOverrideIn);

    void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
              const glm::vec4& viewport, const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);
public:
    uint32_t numBlocksPerWorkgroup = 1024;
protected:
    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
    std::shared_ptr<Program> splatProg;
    std::shared_ptr<Program> preSortProg;
    std::shared_ptr<Program> histogramProg;
    std::shared_ptr<Program> sortProg;
    std::shared_ptr<VertexArrayObject> splatVao;

    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;
    std::vector<uint32_t> atomicCounterVec;

    std::shared_ptr<BufferObject> gaussianDataBuffer;
    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> keyBuffer2;
    std::shared_ptr<BufferObject> histogramBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> valBuffer2;
    std::shared_ptr<BufferObject> posBuffer;
    std::shared_ptr<BufferObject> atomicCounterBuffer;

    uint32_t sortCount;
    bool isFramebufferSRGBEnabled;
    bool useRgcSortOverride;
};
