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

#define SOFTWARE_SORT

#ifdef SOFTWARE_SORT
class SortBuddy;
#endif

class SplatRenderer
{
public:
    SplatRenderer();
    ~SplatRenderer();

    bool Init(std::shared_ptr<GaussianCloud> gaussianCloud, bool isFramebufferSRGBEnabledIn,
              bool useFullSHIn);

    void Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
              const glm::vec4& viewport, const glm::vec2& nearFar);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);
protected:
    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);

    std::shared_ptr<Program> splatProg;
    std::shared_ptr<VertexArrayObject> splatVao;

    bool isFramebufferSRGBEnabled;
    bool useFullSH;
    uint32_t numElements;
    std::vector<glm::vec4> posVec;
    std::vector<uint32_t> indexVec;

#ifndef SOFTWARE_SORT
    std::shared_ptr<Program> preSortProg;

    std::vector<uint32_t> depthVec;
    std::vector<uint32_t> atomicCounterVec;

    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> posBuffer;
    std::shared_ptr<BufferObject> atomicCounterBuffer;

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
    uint32_t sortCount;
#else
    std::shared_ptr<SortBuddy> sortBuddy;
    uint32_t sortId;
#endif
};
