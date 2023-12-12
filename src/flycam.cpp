#include "flycam.h"

#include "core/log.h"

FlyCam::FlyCam(const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn) :
    pos(posIn), vel(0.0f, 0.0f, 0.0f), rot(rotIn), speed(speedIn), rotSpeed(rotSpeedIn)
{

}

void FlyCam::Process(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn, float rollAmountIn, float dt)
{
    glm::vec2 leftStick = glm::clamp(leftStickIn, -1.0f, 1.0f);
    glm::vec2 rightStick = glm::clamp(rightStickIn, -1.0f, 1.0f);
    float rollAmount = glm::clamp(rollAmountIn, -1.0f, 1.0f);

    const float STIFF = 15.0f;
    const float K = STIFF / speed;

    // left stick controls position
    glm::vec3 stick = rot * glm::vec3(leftStick.x, 0.0f, -leftStick.y);
    glm::vec3 s_over_k = (stick * STIFF) / K;
    glm::vec3 s_over_k_sq = (stick * STIFF) / (K * K);
    float e_neg_kt = exp(-K * dt);

    glm::vec3 v = s_over_k + e_neg_kt * (vel - s_over_k);
    pos = s_over_k * dt + (s_over_k_sq - vel / K) * e_neg_kt + pos - s_over_k_sq + (vel / K);
    vel = v;

    // right stick controls orientation
    glm::vec3 up = glm::rotate(rot, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 right = glm::rotate(rot, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 forward = glm::rotate(rot, glm::vec3(0.0f, 0.0f, -1.0f));
    glm::quat yaw = glm::angleAxis(rotSpeed * dt * -rightStick.x, up);
    glm::quat pitch = glm::angleAxis(rotSpeed * dt * rightStick.y, right);
    glm::quat roll = glm::angleAxis(rotSpeed * dt * rollAmount, forward);
    rot = (yaw * pitch * roll) * rot;

    glm::vec3 x = glm::rotate(rot, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 y = glm::rotate(rot, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 z = glm::rotate(rot, glm::vec3(0.0f, 0.0f, 1.0f));
    cameraMat = glm::mat4(glm::vec4(x, 0.0f), glm::vec4(y, 0.0f), glm::vec4(z, 0.0f), glm::vec4(pos, 1.0f));
}

void FlyCam::SetCameraMat(const glm::mat4& cameraMat)
{
    pos = glm::vec3(cameraMat[3]);
    rot = glm::normalize(glm::quat(glm::mat3(cameraMat)));
    vel = glm::vec3(0.0f, 0.0f, 0.0f);
}
