#include "flycam.h"

#include "log.h"

FlyCam::FlyCam(const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn) :
    pos(posIn), rot(rotIn), speed(speedIn), rotSpeed(rotSpeedIn)
{
    ;
}

void FlyCam::SetInput(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn)
{
    leftStick = glm::clamp(leftStickIn, -1.0f, 1.0f);
    rightStick = glm::clamp(rightStickIn, -1.0f, 1.0f);
}

void FlyCam::Process(float dt)
{
    // left stick controls position
    glm::vec3 ls = glm::vec3(leftStick.x, 0.0f, -leftStick.y);
    glm::vec3 v = ls * speed * dt;
    pos += rot * v;

    // right stick controls orientation
    glm::quat yaw = glm::angleAxis(rotSpeed * dt * -rightStick.x, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitch = glm::angleAxis(rotSpeed * dt * rightStick.y, glm::vec3(1.0f, 0.0f, 0.0f));
    rot = rot * (yaw * pitch);

    // TODO: as rot gets rotated around it will accumulate roll, we should cancel that out directly
    // instead of doing gram-smith.  I think I can use swing-twist decomposition to do that...

    // build camera matrix
    // prevent roll by doing gram-smith orthgonalization, where z is the primary axis and (0, 1, 0) is the secondary one.

    glm::vec3 z = rot * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 y = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 x = glm::cross(y, z);
    y = glm::cross(z, x);

    /*
    glm::vec3 z = rot * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 y = rot * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 x = rot * glm::vec3(1.0f, 0.0f, 0.0f);
    */

    cameraMat = glm::mat4(glm::vec4(x, 0.0f),
                          glm::vec4(y, 0.0f),
                          glm::vec4(z, 0.0f),
                          glm::vec4(pos, 1.0f));
}
