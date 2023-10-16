#include "flycam.h"

#include "log.h"

FlyCam::FlyCam(const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn) :
    pos(posIn), rot(rotIn), speed(speedIn), rotSpeed(rotSpeedIn)
{

}

void FlyCam::SetInput(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn, float rollAmountIn)
{
    leftStick = glm::clamp(leftStickIn, -1.0f, 1.0f);
    rightStick = glm::clamp(rightStickIn, -1.0f, 1.0f);
    rollAmount = glm::clamp(rollAmountIn, -1.0f, 1.0f);
}

void FlyCam::Process(float dt)
{
    // left stick controls position
    glm::vec3 ls = glm::vec3(leftStick.x, 0.0f, -leftStick.y);
    glm::vec3 v = ls * speed * dt;
    pos += rot * v;

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
    rot = glm::quat(glm::mat3(cameraMat));
}
