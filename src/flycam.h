#ifndef FLYCAM_H
#define FLYCAM_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class FlyCam
{
public:
    FlyCam(const glm::vec3& posIn, const glm::quat& rotIn, float speedIn, float rotSpeedIn);

    void SetInput(const glm::vec2& leftStickIn, const glm::vec2& rightStickIn);
    void Process(float dt);
    const glm::mat4& GetCameraMat() const { return cameraMat; }

protected:

    float speed;  // units per sec
    float rotSpeed; // radians per sec
    glm::vec2 leftStick;
    glm::vec2 rightStick;
    glm::vec3 pos;
    glm::quat rot;
    glm::mat4 cameraMat;
};

#endif
