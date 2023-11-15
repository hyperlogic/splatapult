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
	size_t GetNumCameras() const { return cameraVec.size(); }

	const glm::vec3& GetCarpetNormal() const { return carpetNormal; }
	const glm::vec3& GetCarpetPos() const { return carpetPos; }
protected:

	void EstimateFloorPlane();

    std::vector<glm::mat4> cameraVec;
	glm::vec3 carpetNormal;
	glm::vec3 carpetPos;
};

#endif
