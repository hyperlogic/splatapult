/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "flycam.h"

#include "core/log.h"
#include "core/util.h"

FlyCam::FlyCam(const glm::vec3& worldUpIn, const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn) :
    worldUp(worldUpIn), pos(posIn), vel(0.0f, 0.0f, 0.0f), rot(rotIn),
    cameraMat(MakeMat4(rot, pos)), speed(speedIn), rotSpeed(rotSpeedIn)
{

}

void FlyCam::Process(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn, float rollAmountIn,
                     float upAmountIn, float dt)
{
    glm::vec2 leftStick = leftStickIn;
    glm::vec2 rightStick = rightStickIn;
    float rollAmount = rollAmountIn;

    const float STIFF = 15.0f;
    const float K = STIFF / speed;

    // left stick controls position
    glm::vec3 stick = rot * glm::vec3(leftStick.x, upAmountIn, -leftStick.y);
    glm::vec3 s_over_k = (stick * STIFF) / K;
    glm::vec3 s_over_k_sq = (stick * STIFF) / (K * K);
    float e_neg_kt = exp(-K * dt);

    glm::vec3 v = s_over_k + e_neg_kt * (vel - s_over_k);
    pos = s_over_k * dt + (s_over_k_sq - vel / K) * e_neg_kt + pos - s_over_k_sq + (vel / K);
    vel = v;

    // right stick controls orientation
    //glm::vec3 up = rot * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = rot * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 forward = rot * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::quat yaw = glm::angleAxis(rotSpeed * dt * -rightStick.x, worldUp);
    glm::quat pitch = glm::angleAxis(rotSpeed * dt * rightStick.y, right);
    rot = (yaw * pitch) * rot;

    // axes of new cameraMat
    glm::vec3 x = rot * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 y = rot * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 z = rot * glm::vec3(0.0f, 0.0f, 1.0f);

    // apply roll to worldUp
    if (fabs(rollAmountIn) > 0.1f)
    {
        worldUp = glm::vec3(cameraMat[1]);
        glm::quat roll = glm::angleAxis(rotSpeed * dt * rollAmount, forward);
        worldUp = roll * worldUp;
    }

    // make sure that cameraMat will be orthogonal, and aligned with world up.
    if (glm::dot(z, worldUp) < 0.999f) // if w are aren't looking stright up.
    {
        glm::vec3 xx = glm::normalize(glm::cross(worldUp, z));
        glm::vec3 yy = glm::normalize(glm::cross(z, xx));
        cameraMat = glm::mat4(glm::vec4(xx, 0.0f), glm::vec4(yy, 0.0f), glm::vec4(z, 0.0f), glm::vec4(pos, 1.0f));
    }
    else
    {
        cameraMat = glm::mat4(glm::vec4(x, 0.0f), glm::vec4(y, 0.0f), glm::vec4(z, 0.0f), glm::vec4(pos, 1.0f));
    }
    glm::vec3 unusedScale;
    Decompose(cameraMat, &unusedScale, &rot);
}

void FlyCam::SetCameraMat(const glm::mat4& cameraMat)
{
    pos = glm::vec3(cameraMat[3]);
    rot = glm::normalize(glm::quat(glm::mat3(cameraMat)));
    vel = glm::vec3(0.0f, 0.0f, 0.0f);
}
