#ifndef DEBUGDRAW_H
#define DEBUGDRAW_H

#include <glm/glm.hpp>

bool DebugDraw_Init();
void DebugDraw_Shutdown();

void DebugDraw_Line(const glm::vec3& start, const glm::vec3& end);
void DebugDraw_Transform(const glm::mat4& m, float axisLen = 1.0f);

void DebugDraw_Render(const glm::mat4& modelViewProjMat);
void DebugDraw_Clear();

#endif
