/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

class CamerasConfig
{
public:
    CamerasConfig();

    bool ImportJson(const std::string& jsonFilename);

    const std::vector<glm::mat4>& GetCameraVec() const { return cameraVec; }
	size_t GetNumCameras() const { return cameraVec.size(); }

	void EstimateFloorPlane(glm::vec3& normalOut, glm::vec3& posOut) const;
protected:

    std::vector<glm::mat4> cameraVec;
};
