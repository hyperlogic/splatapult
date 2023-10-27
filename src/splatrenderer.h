#ifndef SPLATRENDERER_H
#define SPLATRENDERER_H

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

    bool Init(std::shared_ptr<GaussianCloud> GaussianCloud);

    void Sort(const glm::mat4& cameraMat);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);
protected:
    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);

    std::shared_ptr<Program> splatProg;
    std::shared_ptr<Program> preSortProg;
    std::shared_ptr<VertexArrayObject> splatVao;

    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;

    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> posBuffer;

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
};

#endif
