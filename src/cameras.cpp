#include "cameras.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "core/log.h"
#include "core/util.h"

Cameras::Cameras()
{
}

bool Cameras::ImportJson(const std::string& jsonFilename)
{
    std::ifstream f(GetRootPath() + jsonFilename);
    nlohmann::json data = nlohmann::json::parse(f);

    try
    {
        for (auto&& o : data)
        {
            int id = o["id"].template get<int>();

            nlohmann::json jPos = o["position"];
            glm::vec3 pos(jPos[0].template get<float>(), jPos[1].template get<float>(), jPos[2].template get<float>());

            nlohmann::json jRot = o["rotation"];
            glm::mat3 rot(jRot[0][0].template get<float>(), jRot[1][0].template get<float>(), jRot[2][0].template get<float>(),
                          jRot[0][1].template get<float>(), jRot[1][1].template get<float>(), jRot[2][1].template get<float>(),
                          jRot[0][2].template get<float>(), jRot[1][2].template get<float>(), jRot[2][2].template get<float>());

            // swizzle rot to make -z forward and y up.
            cameraVec.emplace_back(glm::mat4(glm::vec4(rot[0], 0.0f),
                                             glm::vec4(-rot[1], 0.0f),
                                             glm::vec4(-rot[2], 0.0f),
                                             glm::vec4(pos, 1.0f)));
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        std::string s = e.what();
        Log::printf("ImportJson exception: %s\n", s.c_str());
        return false;
    }

    return true;
}


