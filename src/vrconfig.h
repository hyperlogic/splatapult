#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

class VrConfig
{
public:
    VrConfig();

    bool ImportJson(const std::string& jsonFilename);
    bool ExportJson(const std::string& jsonFilename) const;
    const glm::mat4& GetFloorMat() const { return floorMat; }
    void SetFloorMat(const glm::mat4& floorMatIn) { floorMat = floorMatIn; }
protected:
    glm::mat4 floorMat;
};
