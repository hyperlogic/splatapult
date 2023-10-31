#include "splatrenderer.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "core/image.h"
#include "core/log.h"
#include "core/texture.h"

#include "radix_sort.hpp"

SplatRenderer::SplatRenderer()
{
}

SplatRenderer::~SplatRenderer()
{
}

bool SplatRenderer::Init(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatProg = std::make_shared<Program>();
    if (!splatProg->LoadVertGeomFrag("shader/splat_vert.glsl", "shader/splat_geom.glsl", "shader/splat_frag.glsl"))
    {
        Log::printf("Error loading splat shaders!\n");
        return false;
    }

    preSortProg = std::make_shared<Program>();
    if (!preSortProg->LoadCompute("shader/presort_compute.glsl"))
    {
        Log::printf("Error loading point pre-sort compute shader!\n");
        return false;
    }

    BuildVertexArrayObject(gaussianCloud);
    depthVec.resize(gaussianCloud->size());
    keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, true);
    valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, true);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);

    sorter = std::make_shared<rgc::radix_sort::sorter>(gaussianCloud->size());

    return true;
}

void SplatRenderer::Sort(const glm::mat4& cameraMat)
{
    const size_t numPoints = posVec.size();

    {
        ZoneScopedNC("depth compute", tracy::Color::Red4);

        // transform forward vector into world space
        glm::vec3 forward = glm::mat3(cameraMat) * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 eye = glm::vec3(cameraMat[3]);

        preSortProg->Bind();
        preSortProg->SetUniform("forward", forward);
        preSortProg->SetUniform("eye", eye);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1, 1); // Assuming LOCAL_SIZE threads per group
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), numPoints);
    }

    {
        ZoneScopedNC("copy sorted indices", tracy::Color::DarkGreen);

        glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        glBindBuffer(GL_COPY_WRITE_BUFFER, splatVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, numPoints * sizeof(uint32_t));
    }
}

void SplatRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ZoneScoped;

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
        splatProg->SetUniform("projParams", glm::vec4(0.0f, nearFar.x, nearFar.y, 0.0f));
        splatProg->SetUniform("viewport", viewport);
        splatProg->SetUniform("eye", eye);

        splatVao->DrawElements(GL_POINTS);
    }
}

void SplatRenderer::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatVao = std::make_shared<VertexArrayObject>();

    // convert gaussianCloud data into buffers
    size_t numPoints = gaussianCloud->size();
    posVec.reserve(numPoints);

    std::vector<glm::vec4> sh0Vec, sh1Vec, sh2Vec, sh3Vec, sh4Vec, sh5Vec, sh6Vec;
    std::vector<glm::vec3> cov3_col0Vec, cov3_col1Vec, cov3_col2Vec;

    sh0Vec.reserve(numPoints);
    sh1Vec.reserve(numPoints);
    sh2Vec.reserve(numPoints);
    sh3Vec.reserve(numPoints);
    sh4Vec.reserve(numPoints);
    sh5Vec.reserve(numPoints);
    sh6Vec.reserve(numPoints);
    cov3_col0Vec.reserve(numPoints);
    cov3_col1Vec.reserve(numPoints);
    cov3_col2Vec.reserve(numPoints);

    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        // stick alpha into position.w
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        posVec.emplace_back(glm::vec4(g.position[0], g.position[1], g.position[2], alpha));

        sh0Vec.emplace_back(glm::vec4(g.f_dc[0], g.f_dc[1], g.f_dc[2], g.f_rest[0]));
        sh1Vec.emplace_back(glm::vec4(g.f_rest[1], g.f_rest[2], g.f_rest[3], g.f_rest[4]));
        sh2Vec.emplace_back(glm::vec4(g.f_rest[5], g.f_rest[6], g.f_rest[7], g.f_rest[8]));
        sh3Vec.emplace_back(glm::vec4(g.f_rest[9], g.f_rest[10], g.f_rest[11], g.f_rest[12]));
        sh4Vec.emplace_back(glm::vec4(g.f_rest[13], g.f_rest[14], g.f_rest[15], g.f_rest[16]));
        sh5Vec.emplace_back(glm::vec4(g.f_rest[17], g.f_rest[18], g.f_rest[19], g.f_rest[20]));
        sh6Vec.emplace_back(glm::vec4(g.f_rest[21], g.f_rest[22], g.f_rest[23], 0.0f));

        glm::mat3 V = g.ComputeCovMat();
        cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]);
    }
    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto sh0Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh0Vec);
    auto sh1Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh1Vec);
    auto sh2Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh2Vec);
    auto sh3Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh3Vec);
    auto sh4Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh4Vec);
    auto sh5Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh5Vec);
    auto sh6Buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, sh6Vec);
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
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, true); // dynamic

    // setup vertex array object with buffers
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("position"), positionBuffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh0"), sh0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh1"), sh1Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh2"), sh2Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh3"), sh3Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh4"), sh4Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh5"), sh5Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("sh6"), sh6Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col0"), cov3_col0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col1"), cov3_col1Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col2"), cov3_col2Buffer);
    splatVao->SetElementBuffer(indexBuffer);
}
