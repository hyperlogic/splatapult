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

#ifndef __ANDROID__
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

SplatRenderer::SplatRenderer()
{
}

SplatRenderer::~SplatRenderer()
{
}

bool SplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud, bool isFramebufferSRGBEnabledIn,
                         bool useFullSHIn)
{
    GL_ERROR_CHECK("SplatRenderer::Init() begin");

    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;
    useFullSH = useFullSHIn;

    splatProg = std::make_shared<Program>();
    if (isFramebufferSRGBEnabled || useFullSH)
    {
        std::string defines = "";
        if (isFramebufferSRGBEnabled)
        {
            defines += "#define FRAMEBUFFER_SRGB\n";
        }
        if (useFullSH)
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
        Log::E("Error loading point pre-sort compute shader!\n");
        return false;
    }

    BuildVertexArrayObject(gaussianCloud);
    depthVec.resize(gaussianCloud->size());
    keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
    valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);

    sorter = std::make_shared<rgc::radix_sort::sorter>(gaussianCloud->size());

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

    {
        ZoneScopedNC("pre-sort", tracy::Color::Red4);

        preSortProg->Bind();
        preSortProg->SetUniform("modelViewProj", projMat * modelViewMat);

        // reset counter back to zero
        atomicCounterVec[0] = 0;
        atomicCounterBuffer->Update(atomicCounterVec);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
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

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);

        GL_ERROR_CHECK("SplatRenderer::Sort() sort");
    }

    {
        ZoneScopedNC("copy-sorted", tracy::Color::DarkGreen);

        glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
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

    // convert gaussianCloud data into buffers
    size_t numPoints = gaussianCloud->size();
    posVec.reserve(numPoints);

    // sh coeff
    std::vector<glm::vec4> r_sh0Vec, g_sh0Vec, b_sh0Vec;
    std::vector<glm::vec4> r_sh1Vec, r_sh2Vec, r_sh3Vec, r_sh4Vec;
    std::vector<glm::vec4> g_sh1Vec, g_sh2Vec, g_sh3Vec, g_sh4Vec;
    std::vector<glm::vec4> b_sh1Vec, b_sh2Vec, b_sh3Vec, b_sh4Vec;

    // 3x3 cov matrix
    std::vector<glm::vec3> cov3_col0Vec, cov3_col1Vec, cov3_col2Vec;

    r_sh0Vec.reserve(numPoints);
    g_sh0Vec.reserve(numPoints);
    b_sh0Vec.reserve(numPoints);

    if (useFullSH)
    {
        r_sh1Vec.reserve(numPoints);
        r_sh2Vec.reserve(numPoints);
        r_sh3Vec.reserve(numPoints);
        g_sh1Vec.reserve(numPoints);
        g_sh2Vec.reserve(numPoints);
        g_sh3Vec.reserve(numPoints);
        b_sh1Vec.reserve(numPoints);
        b_sh2Vec.reserve(numPoints);
        b_sh3Vec.reserve(numPoints);
    }

    cov3_col0Vec.reserve(numPoints);
    cov3_col1Vec.reserve(numPoints);
    cov3_col2Vec.reserve(numPoints);

    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        // stick alpha into position.w
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        posVec.emplace_back(glm::vec4(g.position[0], g.position[1], g.position[2], alpha));

        r_sh0Vec.emplace_back(glm::vec4(g.f_dc[0], g.f_rest[0], g.f_rest[1], g.f_rest[2]));
        g_sh0Vec.emplace_back(glm::vec4(g.f_dc[1], g.f_rest[15], g.f_rest[16], g.f_rest[17]));
        b_sh0Vec.emplace_back(glm::vec4(g.f_dc[2], g.f_rest[30], g.f_rest[31], g.f_rest[32]));

        if (useFullSH)
        {
            r_sh1Vec.emplace_back(glm::vec4(g.f_rest[3], g.f_rest[4], g.f_rest[5], g.f_rest[6]));
            r_sh2Vec.emplace_back(glm::vec4(g.f_rest[7], g.f_rest[8], g.f_rest[9], g.f_rest[10]));
            r_sh3Vec.emplace_back(glm::vec4(g.f_rest[11], g.f_rest[12], g.f_rest[13], g.f_rest[14]));
            g_sh1Vec.emplace_back(glm::vec4(g.f_rest[18], g.f_rest[19], g.f_rest[20], g.f_rest[21]));
            g_sh2Vec.emplace_back(glm::vec4(g.f_rest[22], g.f_rest[23], g.f_rest[24], g.f_rest[25]));
            g_sh3Vec.emplace_back(glm::vec4(g.f_rest[26], g.f_rest[27], g.f_rest[28], g.f_rest[29]));
            b_sh1Vec.emplace_back(glm::vec4(g.f_rest[33], g.f_rest[34], g.f_rest[35], g.f_rest[36]));
            b_sh2Vec.emplace_back(glm::vec4(g.f_rest[37], g.f_rest[38], g.f_rest[39], g.f_rest[40]));
            b_sh3Vec.emplace_back(glm::vec4(g.f_rest[41], g.f_rest[42], g.f_rest[43], g.f_rest[44]));
        }

        glm::mat3 V = g.ComputeCovMat();
        cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]);
    }
    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);

    auto r_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh0Vec);
    auto g_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh0Vec);
    auto b_sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh0Vec);

    auto r_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh1Vec);
    auto r_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh2Vec);
    auto r_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, r_sh3Vec);
    auto g_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh1Vec);
    auto g_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh2Vec);
    auto g_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, g_sh3Vec);
    auto b_sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh1Vec);
    auto b_sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh2Vec);
    auto b_sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, b_sh3Vec);

    auto cov3_col0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col0Vec);
    auto cov3_col1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col1Vec);
    auto cov3_col2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, cov3_col2Vec);

    // build element array
    indexVec.reserve(numPoints);
    assert(numPoints <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

    // setup vertex array object with buffers
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("position"), positionBuffer);

    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh0"), r_sh0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh0"), g_sh0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh0"), b_sh0Buffer);

    if (useFullSH)
    {
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh1"), r_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh2"), r_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("r_sh3"), r_sh3Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh1"), g_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh2"), g_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("g_sh3"), g_sh3Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh1"), b_sh1Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh2"), b_sh2Buffer);
        splatVao->SetAttribBuffer(splatProg->GetAttribLoc("b_sh3"), b_sh3Buffer);
    }

    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col0"), cov3_col0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col1"), cov3_col1Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col2"), cov3_col2Buffer);
    splatVao->SetElementBuffer(indexBuffer);
}
