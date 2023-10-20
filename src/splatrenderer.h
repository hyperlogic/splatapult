#ifndef SPLATRENDERER_H
#define SPLATRENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "gaussiancloud.h"
#include "program.h"
#include "vertexbuffer.h"

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

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::vec4& viewport,
                const glm::vec2& nearFar, float fovy);
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
