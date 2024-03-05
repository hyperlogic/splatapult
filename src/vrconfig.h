/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
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
