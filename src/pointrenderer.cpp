/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "pointrenderer.h"

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

PointRenderer::PointRenderer()
{
}

PointRenderer::~PointRenderer()
{
}

bool PointRenderer::Init(std::shared_ptr<PointCloud> pointCloud, bool isFramebufferSRGBEnabledIn)
{
    GL_ERROR_CHECK("PointRenderer::Init() begin");

    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;

    Image pointImg;
    if (!pointImg.Load("texture/sphere.png"))
    {
        Log::E("Error loading sphere.png\n");
        return false;
    }
    pointImg.isSRGB = isFramebufferSRGBEnabled;

    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};
    pointTex = std::make_shared<Texture>(pointImg, texParams);

    pointProg = std::make_shared<Program>();
    if (!pointProg->LoadVertGeomFrag("shader/point_vert.glsl", "shader/point_geom.glsl", "shader/point_frag.glsl"))
    {
        Log::E("Error loading point shaders!\n");
        return false;
    }

    preSortProg = std::make_shared<Program>();
    if (!preSortProg->LoadCompute("shader/presort_compute.glsl"))
    {
        Log::E("Error loading point pre-sort compute shader!\n");
        return false;
    }

    BuildVertexArrayObject(pointCloud);

    depthVec.resize(pointCloud->size());
    keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
    valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
    sorter = std::make_shared<rgc::radix_sort::sorter>(pointCloud->size());

    atomicCounterVec.resize(1, 0);
    atomicCounterBuffer = std::make_shared<BufferObject>(GL_ATOMIC_COUNTER_BUFFER, atomicCounterVec, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    GL_ERROR_CHECK("PointRenderer::Init() end");

    return true;
}

void PointRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

    GL_ERROR_CHECK("PointRenderer::Render() begin");

    const size_t numPoints = posVec.size();
    glm::mat4 modelViewMat = glm::inverse(cameraMat);

    const uint32_t MAX_DEPTH = std::numeric_limits<uint32_t>::max();

    {
        ZoneScopedNC("pre-sort", tracy::Color::Red4);

        preSortProg->Bind();
        preSortProg->SetUniform("modelViewProj", projMat * modelViewMat);
        preSortProg->SetUniform("nearFar", nearFar);
        preSortProg->SetUniform("keyMax", MAX_DEPTH);

        glm::mat4 modelViewProjMat = projMat * modelViewMat;

        // reset counter back to 0
        atomicCounterVec[0] = 0;
        atomicCounterBuffer->Update(atomicCounterVec);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, atomicCounterBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1, 1); // Assuming LOCAL_SIZE threads per group
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

        GL_ERROR_CHECK("PointRenderer::Render() pre-sort");
    }

    uint32_t sortCount = 0;
    {
        ZoneScopedNC("get-count", tracy::Color::Green);

        atomicCounterBuffer->Read(atomicCounterVec);
        sortCount = atomicCounterVec[0];

        assert(sortCount <= (uint32_t)numPoints);

        GL_ERROR_CHECK("PointRenderer::Render() get-count");
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);

        GL_ERROR_CHECK("PointRenderer::Render() sort");
    }

    {
        ZoneScopedNC("copy-sorted", tracy::Color::DarkGreen);

        glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        glBindBuffer(GL_COPY_WRITE_BUFFER, pointVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sortCount * sizeof(uint32_t));

        GL_ERROR_CHECK("PointRenderer::Render() copy-sorted");
    }

    {
        ZoneScopedNC("draw", tracy::Color::Red4);

        float width = viewport.z;
        float height = viewport.w;
        float aspectRatio = width / height;

        pointProg->Bind();
        pointProg->SetUniform("modelViewMat", modelViewMat);
        pointProg->SetUniform("projMat", projMat);
        pointProg->SetUniform("pointSize", 0.02f);  // in ndc space?!?
        pointProg->SetUniform("invAspectRatio", 1.0f / aspectRatio);

        // use texture unit 0 for colorTexture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pointTex->texture);
        pointProg->SetUniform("colorTex", 0);

        pointVao->Bind();
        glDrawElements(GL_POINTS, sortCount, GL_UNSIGNED_INT, nullptr);
        pointVao->Unbind();

        GL_ERROR_CHECK("PointRenderer::Render() draw");
    }
}

void PointRenderer::BuildVertexArrayObject(std::shared_ptr<PointCloud> pointCloud)
{
    pointVao = std::make_shared<VertexArrayObject>();

    // convert pointCloud positions and colors into buffers
    size_t numPoints = pointCloud->size();
    posVec.reserve(numPoints);

    std::vector<glm::vec4> colorVec;
    colorVec.reserve(numPoints);
    for (auto&& p : pointCloud->GetPointVec())
    {
        posVec.emplace_back(glm::vec4(p.position[0], p.position[1], p.position[2], 1.0f));
        colorVec.emplace_back(glm::vec4(p.color[0] / 255.0f, p.color[1] / 255.0f, p.color[2] / 255.0f, 1.0f));
    }

    if (isFramebufferSRGBEnabled)
    {
        // convert all colors to linear space
        for (auto&& c : colorVec)
        {
            c = SRGBToLinear(c);
        }
    }

    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto colorBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);

    // build element array
    indexVec.reserve(numPoints);
    assert(numPoints <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

    // setup vertex array object with buffers
    pointVao->SetAttribBuffer(pointProg->GetAttribLoc("position"), positionBuffer);
    pointVao->SetAttribBuffer(pointProg->GetAttribLoc("color"), colorBuffer);
    pointVao->SetElementBuffer(indexBuffer);
}
