/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "splatrenderer.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "core/image.h"
#include "core/log.h"
#include "core/texture.h"
#include "core/util.h"

#include "radix_sort.hpp"

static const uint32_t NUM_BLOCKS_PER_WORKGROUP = 1024;

static void SetupAttrib(int loc, const BinaryAttribute& attrib, int32_t count, size_t stride)
{
    assert(attrib.type == BinaryAttribute::Type::Float);
    glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, (uint32_t)stride, (void*)attrib.offset);
    glEnableVertexAttribArray(loc);
}

SplatRenderer::SplatRenderer()
{
}

SplatRenderer::~SplatRenderer()
{
}

bool SplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud,
                         bool isFramebufferSRGBEnabledIn, bool useRgcSortOverrideIn)
{
    ZoneScopedNC("SplatRenderer::Init()", tracy::Color::Blue);
    GL_ERROR_CHECK("SplatRenderer::Init() begin");

    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;
    useRgcSortOverride = useRgcSortOverrideIn;

    splatProg = std::make_shared<Program>();
    if (isFramebufferSRGBEnabled || gaussianCloud->HasFullSH())
    {
        std::string defines = "";
        if (isFramebufferSRGBEnabled)
        {
            defines += "#define FRAMEBUFFER_SRGB\n";
        }
        if (gaussianCloud->HasFullSH())
        {
            defines += "#define FULL_SH\n";
        }
        splatProg->AddMacro("DEFINES", defines);
    }
    if (!splatProg->LoadVertGeomFrag("shader/splat_vert.glsl", "shader/splat_geom.glsl", "shader/splat_frag.glsl"))
    {
        Log::E("Error loading splat shaders!\n");
        return false;
    }

    preSortProg = std::make_shared<Program>();
    if (!preSortProg->LoadCompute("shader/presort_compute.glsl"))
    {
        Log::E("Error loading pre-sort compute shader!\n");
        return false;
    }

    bool useMultiRadixSort = GLEW_KHR_shader_subgroup && !useRgcSortOverride;

    if (useMultiRadixSort)
    {
        sortProg = std::make_shared<Program>();
        if (!sortProg->LoadCompute("shader/multi_radixsort.glsl"))
        {
            Log::E("Error loading sort compute shader!\n");
            return false;
        }

        histogramProg = std::make_shared<Program>();
        if (!histogramProg->LoadCompute("shader/multi_radixsort_histograms.glsl"))
        {
            Log::E("Error loading histogram compute shader!\n");
            return false;
        }
    }

    // build posVec
    size_t numGaussians = gaussianCloud->GetNumGaussians();
    posVec.reserve(numGaussians);
    gaussianCloud->ForEachPosWithAlpha([this](const float* pos)
    {
        posVec.emplace_back(glm::vec4(pos[0], pos[1], pos[2], 1.0f));
    });

    BuildVertexArrayObject(gaussianCloud);

    depthVec.resize(numGaussians);

    if (useMultiRadixSort)
    {
        Log::I("using multi_radixsort.glsl\n");

        keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
        keyBuffer2 = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);

        const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(numGaussians);
        const uint32_t NUM_WORKGROUPS = (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;
        const uint32_t RADIX_SORT_BINS = 256;

        std::vector<uint32_t> histogramVec(NUM_WORKGROUPS * RADIX_SORT_BINS, 0);
        histogramBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, histogramVec, GL_DYNAMIC_STORAGE_BIT);

        valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        valBuffer2 = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
    }
    else
    {
        Log::I("using rgc::radix_sort\n");
        keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
        valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
        posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);

        sorter = std::make_shared<rgc::radix_sort::sorter>(numGaussians);
    }

    atomicCounterVec.resize(1, 0);
    atomicCounterBuffer = std::make_shared<BufferObject>(GL_ATOMIC_COUNTER_BUFFER, atomicCounterVec, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    GL_ERROR_CHECK("SplatRenderer::Init() end");

    return true;
}

void SplatRenderer::Sort(const glm::mat4& cameraMat, const glm::mat4& projMat,
                         const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

    GL_ERROR_CHECK("SplatRenderer::Sort() begin");

    const size_t numPoints = posVec.size();
    glm::mat4 modelViewMat = glm::inverse(cameraMat);

    bool useMultiRadixSort = GLEW_KHR_shader_subgroup && !useRgcSortOverride;

    // 24 bit radix sort still has some artifacts on some datasets, so use 32 bit sort.
    //const uint32_t NUM_BYTES = useMultiRadixSort ? 3 : 4;
    //const uint32_t MAX_DEPTH = useMultiRadixSort ? 16777215 : std::numeric_limits<uint32_t>::max();
    const uint32_t NUM_BYTES = 4;
    const uint32_t MAX_DEPTH = std::numeric_limits<uint32_t>::max();

    {
        ZoneScopedNC("pre-sort", tracy::Color::Red4);

        preSortProg->Bind();
        preSortProg->SetUniform("modelViewProj", projMat * modelViewMat);
        preSortProg->SetUniform("nearFar", nearFar);
        preSortProg->SetUniform("keyMax", MAX_DEPTH);

        // reset counter back to zero
        atomicCounterVec[0] = 0;
        atomicCounterBuffer->Update(atomicCounterVec);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());  // readonly
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());  // writeonly
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());  // writeonly
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, atomicCounterBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1, 1); // Assuming LOCAL_SIZE threads per group
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

        GL_ERROR_CHECK("SplatRenderer::Sort() pre-sort");
    }

    {
        ZoneScopedNC("get-count", tracy::Color::Green);

        atomicCounterBuffer->Read(atomicCounterVec);
        sortCount = atomicCounterVec[0];

        assert(sortCount <= (uint32_t)numPoints);

        GL_ERROR_CHECK("SplatRenderer::Render() get-count");
    }

    if (useMultiRadixSort)
    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(sortCount);
        const uint32_t NUM_WORKGROUPS = (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;

        sortProg->Bind();
        sortProg->SetUniform("g_num_elements", NUM_ELEMENTS);
        sortProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
        sortProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

        histogramProg->Bind();
        histogramProg->SetUniform("g_num_elements", NUM_ELEMENTS);
        //histogramProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
        histogramProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

        for (uint32_t i = 0; i < NUM_BYTES; i++)
        {
            histogramProg->Bind();
            histogramProg->SetUniform("g_shift", 8 * i);

            if (i == 0 || i == 2)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
            }
            else
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogramBuffer->GetObj());

            glDispatchCompute(NUM_WORKGROUPS, 1, 1);

            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            sortProg->Bind();
            sortProg->SetUniform("g_shift", 8 * i);

            if ((i % 2) == 0)  // even
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer2->GetObj());
            }
            else  // odd
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer2->GetObj());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer->GetObj());
            }
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, histogramBuffer->GetObj());

            glDispatchCompute(NUM_WORKGROUPS, 1, 1);

            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        GL_ERROR_CHECK("SplatRenderer::Sort() sort");

        // indicate if keys are sorted properly or not.
        if (false)
        {
            std::vector<uint32_t> sortedKeyVec(numPoints, 0);
            keyBuffer->Read(sortedKeyVec);

            GL_ERROR_CHECK("SplatRenderer::Sort() READ buffer");

            bool sorted = true;
            for (uint32_t i = 1; i < sortCount; i++)
            {
                if (sortedKeyVec[i - 1] > sortedKeyVec[i])
                {
                    sorted = false;
                    break;
                }
            }

            printf("%s", sorted ? "o" : "x");
        }
    }
    else
    {
        ZoneScopedNC("sort", tracy::Color::Red4);
        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);
        GL_ERROR_CHECK("SplatRenderer::Sort() rgc sort");
    }

    {
        ZoneScopedNC("copy-sorted", tracy::Color::DarkGreen);

        if (useMultiRadixSort && (NUM_BYTES % 2) == 1)  // odd
        {
            glBindBuffer(GL_COPY_READ_BUFFER, valBuffer2->GetObj());
        }
        else
        {
            glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        }
        glBindBuffer(GL_COPY_WRITE_BUFFER, splatVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sortCount * sizeof(uint32_t));

        GL_ERROR_CHECK("SplatRenderer::Sort() copy-sorted");
    }
}


void SplatRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

    GL_ERROR_CHECK("SplatRenderer::Render() begin");

    {
        ZoneScopedNC("draw", tracy::Color::Red4);
        float width = viewport.z;
        float height = viewport.w;
        float aspectRatio = width / height;
        glm::mat4 viewMat = glm::inverse(cameraMat);
        glm::vec3 eye = glm::vec3(cameraMat[3]);

        splatProg->Bind();
        splatProg->SetUniform("viewMat", viewMat);
        splatProg->SetUniform("projMat", projMat);
        splatProg->SetUniform("viewport", viewport);
        splatProg->SetUniform("projParams", glm::vec4(0.0f, nearFar.x, nearFar.y, 0.0f));
        splatProg->SetUniform("eye", eye);

        splatVao->Bind();
        glDrawElements(GL_POINTS, sortCount, GL_UNSIGNED_INT, nullptr);
        splatVao->Unbind();

        GL_ERROR_CHECK("SplatRenderer::Render() draw");
    }
}

void SplatRenderer::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatVao = std::make_shared<VertexArrayObject>();

    // allocate large buffer to hold interleaved vertex data
    gaussianDataBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER,
                                                        gaussianCloud->GetRawDataPtr(),
                                                        gaussianCloud->GetTotalSize(), 0);

    const size_t numGaussians = gaussianCloud->GetNumGaussians();

    // build element array
    indexVec.reserve(numGaussians);
    assert(numGaussians <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numGaussians; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

    splatVao->Bind();
    gaussianDataBuffer->Bind();

    const size_t stride = gaussianCloud->GetStride();
    SetupAttrib(splatProg->GetAttribLoc("position"), gaussianCloud->GetPosWithAlphaAttrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("r_sh0"), gaussianCloud->GetR_SH0Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("g_sh0"), gaussianCloud->GetG_SH0Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("b_sh0"), gaussianCloud->GetB_SH0Attrib(), 4, stride);
    if (gaussianCloud->HasFullSH())
    {
        SetupAttrib(splatProg->GetAttribLoc("r_sh1"), gaussianCloud->GetR_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("r_sh2"), gaussianCloud->GetR_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("r_sh3"), gaussianCloud->GetR_SH3Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh1"), gaussianCloud->GetG_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh2"), gaussianCloud->GetG_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("g_sh3"), gaussianCloud->GetG_SH3Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh1"), gaussianCloud->GetB_SH1Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh2"), gaussianCloud->GetB_SH2Attrib(), 4, stride);
        SetupAttrib(splatProg->GetAttribLoc("b_sh3"), gaussianCloud->GetB_SH3Attrib(), 4, stride);
    }
    SetupAttrib(splatProg->GetAttribLoc("cov3_col0"), gaussianCloud->GetCov3_Col0Attrib(), 3, stride);
    SetupAttrib(splatProg->GetAttribLoc("cov3_col1"), gaussianCloud->GetCov3_Col1Attrib(), 3, stride);
    SetupAttrib(splatProg->GetAttribLoc("cov3_col2"), gaussianCloud->GetCov3_Col2Attrib(), 3, stride);

    splatVao->SetElementBuffer(indexBuffer);
    gaussianDataBuffer->Unbind();
}
