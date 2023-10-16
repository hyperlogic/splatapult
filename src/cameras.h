#ifndef CAMERAS_H
#define CAMERAS_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

class Cameras
{
public:
    Cameras();

    bool ImportJson(const std::string& jsonFilename);

    const std::vector<glm::mat4>& GetCameraVec() const { return cameraVec; }

protected:

    std::vector<glm::mat4> cameraVec;
};

#endif
