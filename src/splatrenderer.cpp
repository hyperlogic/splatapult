#include "splatrenderer.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "image.h"
#include "log.h"
#include "texture.h"
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

        splatProg->Bind();
        splatProg->SetUniform("viewMat", viewMat);
        splatProg->SetUniform("projMat", projMat);
        splatProg->SetUniform("projParams", glm::vec4(0.0f, nearFar.x, nearFar.y, 0.0f));
        splatProg->SetUniform("viewport", viewport);

        splatVao->DrawElements(GL_POINTS);
    }
}

void SplatRenderer::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud)
{
    splatVao = std::make_shared<VertexArrayObject>();

    // convert gaussianCloud data into buffers
    size_t numPoints = gaussianCloud->size();
    posVec.reserve(numPoints);

    std::vector<glm::vec4> colorVec;
    std::vector<glm::vec3> cov3_col0Vec;
    std::vector<glm::vec3> cov3_col1Vec;
    std::vector<glm::vec3> cov3_col2Vec;

    colorVec.reserve(numPoints);
    cov3_col0Vec.reserve(numPoints);
    cov3_col1Vec.reserve(numPoints);
    cov3_col2Vec.reserve(numPoints);

    for (auto&& g : gaussianCloud->GetGaussianVec())
    {
        posVec.emplace_back(glm::vec4(g.position[0], g.position[1], g.position[2], 1.0f));

        const float SH_C0 = 0.28209479177387814f;
        float alpha = 1.0f / (1.0f + expf(-g.opacity));
        glm::vec4 color(0.5f + SH_C0 * g.f_dc[0],
                        0.5f + SH_C0 * g.f_dc[1],
                        0.5f + SH_C0 * g.f_dc[2], alpha);
        colorVec.push_back(color);

        glm::mat3 V = g.ComputeCovMat();

        cov3_col0Vec.push_back(V[0]);
        cov3_col1Vec.push_back(V[1]);
        cov3_col2Vec.push_back(V[2]);
    }
    auto positionBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
    auto colorBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);
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
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("color"), colorBuffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col0"), cov3_col0Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col1"), cov3_col1Buffer);
    splatVao->SetAttribBuffer(splatProg->GetAttribLoc("cov3_col2"), cov3_col2Buffer);
    splatVao->SetElementBuffer(indexBuffer);
}
