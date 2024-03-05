/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class FlyCam
{
public:
    FlyCam(const glm::vec3& worldUpIn, const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn);

    void Process(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn, float rollAmountIn,
                 float upAmountIn, float dt);
    const glm::mat4& GetCameraMat() const { return cameraMat; }
    void SetCameraMat(const glm::mat4& cameraMat);

protected:

    float speed;  // units per sec
    float rotSpeed; // radians per sec
    glm::vec3 worldUp;
    glm::vec3 pos;
    glm::vec3 vel;
    glm::quat rot;
    glm::mat4 cameraMat;
};
