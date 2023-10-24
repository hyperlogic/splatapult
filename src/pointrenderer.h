#ifndef POINTRENDERER_H
#define POINTRENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

#include "pointcloud.h"
#include "program.h"
#include "texture.h"
#include "vertexbuffer.h"

namespace rgc::radix_sort
{
    struct sorter;
}

class PointRenderer
{
public:
    PointRenderer();
    ~PointRenderer();

    bool Init(std::shared_ptr<PointCloud> pointCloud);

    // viewport = (x, y, width, height)
    void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);
protected:
    void BuildVertexArrayObject(std::shared_ptr<PointCloud> pointCloud);

    std::shared_ptr<Texture> pointTex;
    std::shared_ptr<Program> pointProg;
    std::shared_ptr<Program> preSortProg;
    std::shared_ptr<VertexArrayObject> pointVao;

    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;

    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> posBuffer;

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
};

#endif
