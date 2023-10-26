#include "debugdraw.h"

#include <GL/glew.h>
#include <memory>
#include <vector>

#include "log.h"
#include "util.h"
#include "program.h"

static std::shared_ptr<Program> ddProg;
static std::vector<glm::vec3> linePositionVec;
static std::vector<glm::vec3> lineColorVec;

bool DebugDraw_Init()
{
    ddProg = std::make_shared<Program>();
    if (!ddProg->LoadVertFrag("shader/debugdraw_vert.glsl", "shader/debugdraw_frag.glsl"))
    {
        Log::printf("Error loading debugdraw shaders!\n");
        return false;
    }
    return true;
}

void DebugDraw_Shutdown()
{
    ddProg.reset();
}

void DebugDraw_Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3 color)
{
    linePositionVec.push_back(start);
    linePositionVec.push_back(end);
    lineColorVec.push_back(color);
    lineColorVec.push_back(color);
}

void DebugDraw_Transform(const glm::mat4& m, float axisLen)
{
    glm::vec3 x = glm::vec3(m[0]);
    glm::vec3 y = glm::vec3(m[1]);
    glm::vec3 z = glm::vec3(m[2]);
    x = axisLen * glm::normalize(x);
    y = axisLen * glm::normalize(y);
    z = axisLen * glm::normalize(z);
    glm::vec3 p = glm::vec3(m[3]);

    DebugDraw_Line(p, p + x, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    DebugDraw_Line(p, p + y, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    DebugDraw_Line(p, p + z, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugDraw_Render(const glm::mat4& modelViewProjMat)
{
    ddProg->Bind();
    ddProg->SetUniform("modelViewProjMat", modelViewProjMat);
    ddProg->SetAttrib("position", linePositionVec.data());
    ddProg->SetAttrib("color", lineColorVec.data());
    glDrawArrays(GL_LINES, 0, (GLsizei)linePositionVec.size());
}

void DebugDraw_Clear()
{
    linePositionVec.clear();
    lineColorVec.clear();
}
