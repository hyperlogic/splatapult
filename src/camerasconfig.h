/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Camera
{
	glm::mat4 mat;  // inverse view matrix
	glm::vec2 fov;
};

class CamerasConfig
{
public:
    CamerasConfig();

    bool ImportJson(const std::string& jsonFilename);

    const std::vector<Camera>& GetCameraVec() const { return cameraVec; }
	size_t GetNumCameras() const { return cameraVec.size(); }

	void EstimateFloorPlane(glm::vec3& normalOut, glm::vec3& posOut) const;
protected:

    std::vector<Camera> cameraVec;
};
