/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "camerasconfig.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

#include "core/log.h"
#include "core/util.h"

CamerasConfig::CamerasConfig()
{

}

bool CamerasConfig::ImportJson(const std::string& jsonFilename)
{
    std::ifstream f(jsonFilename);
    if (f.fail())
    {
        return false;
    }

    try
    {
        nlohmann::json data = nlohmann::json::parse(f);
        for (auto&& o : data)
        {
            int id = o["id"].template get<int>();

            nlohmann::json jPos = o["position"];
            glm::vec3 pos(jPos[0].template get<float>(), jPos[1].template get<float>(), jPos[2].template get<float>());

            nlohmann::json jRot = o["rotation"];
            glm::mat3 rot(jRot[0][0].template get<float>(), jRot[1][0].template get<float>(), jRot[2][0].template get<float>(),
                          jRot[0][1].template get<float>(), jRot[1][1].template get<float>(), jRot[2][1].template get<float>(),
                          jRot[0][2].template get<float>(), jRot[1][2].template get<float>(), jRot[2][2].template get<float>());
            int width = o["width"].template get<int>();
            int height = o["height"].template get<int>();
            float fx = o["fx"].template get<float>();
            float fy = o["fy"].template get<float>();

            glm::vec2 fov(2.0f * atanf(width / (2.0f * fx)),
                          2.0f * atanf(height / (2.0f * fx)));

            glm::mat4 mat(glm::vec4(rot[0], 0.0f),
                          glm::vec4(-rot[1], 0.0f),
                          glm::vec4(-rot[2], 0.0f),
                          glm::vec4(pos, 1.0f));

            // swizzle rot to make -z forward and y up.
            cameraVec.emplace_back(Camera{mat, fov});
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        std::string s = e.what();
        Log::E("CamerasConfig::ImportJson exception: %s\n", s.c_str());
        return false;
    }

    return true;
}

void CamerasConfig::EstimateFloorPlane(glm::vec3& normalOut, glm::vec3& posOut) const
{
    if (cameraVec.empty())
    {
        normalOut = glm::vec3(0.0f, 1.0f, 0.0f);
        posOut = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    glm::vec3 avgUp(0.0f, 0.0f, 0.0f);
    float weight = 1.0f / (float)cameraVec.size();
    for (auto&& c : cameraVec)
    {
        glm::vec3 up(c.mat[1]);
        avgUp += weight * up;
    }
    avgUp = SafeNormalize(avgUp, glm::vec3(0.0f, 1.0f, 0.0f));

    float avgDist = 0.0f;
    for (auto&& c : cameraVec)
    {
        glm::vec3 pos(c.mat[3]);
        avgDist += weight * glm::dot(pos, avgUp);
    }

    normalOut = avgUp;
    posOut = avgUp * avgDist;
}
