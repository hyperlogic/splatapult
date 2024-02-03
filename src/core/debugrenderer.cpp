/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "debugrenderer.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif

#include <memory>
#include <vector>

#include "log.h"
#include "util.h"
#include "program.h"

DebugRenderer::DebugRenderer()
{

}

bool DebugRenderer::Init()
{
    ddProg = std::make_shared<Program>();
    if (!ddProg->LoadVertFrag("shader/debugdraw_vert.glsl", "shader/debugdraw_frag.glsl"))
    {
        Log::E("Error loading DebugRenderer shader!\n");
        return false;
    }
    return true;
}

void DebugRenderer::Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    linePositionVec.push_back(start);
    linePositionVec.push_back(end);
    lineColorVec.push_back(color);
    lineColorVec.push_back(color);
}

void DebugRenderer::Transform(const glm::mat4& m, float axisLen)
{
    glm::vec3 x = glm::vec3(m[0]);
    glm::vec3 y = glm::vec3(m[1]);
    glm::vec3 z = glm::vec3(m[2]);
    x = axisLen * glm::normalize(x);
    y = axisLen * glm::normalize(y);
    z = axisLen * glm::normalize(z);
    glm::vec3 p = glm::vec3(m[3]);

    Line(p, p + x, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    Line(p, p + y, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    Line(p, p + z, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ddProg->Bind();
    glm::mat4 modelViewProjMat = projMat * glm::inverse(cameraMat);
    ddProg->SetUniform("modelViewProjMat", modelViewProjMat);
    ddProg->SetAttrib("position", linePositionVec.data());
    ddProg->SetAttrib("color", lineColorVec.data());
    glDrawArrays(GL_LINES, 0, (GLsizei)linePositionVec.size());
}

void DebugRenderer::EndFrame()
{
    linePositionVec.clear();
    lineColorVec.clear();
}
