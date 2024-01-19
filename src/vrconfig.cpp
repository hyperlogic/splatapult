/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "vrconfig.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

#include "core/log.h"
#include "core/util.h"

VrConfig::VrConfig() : floorMat(1.0f)
{
    ;
}

bool VrConfig::ImportJson(const std::string& jsonFilename)
{
    std::ifstream f(jsonFilename);
    if (f.fail())
    {
        return false;
    }

    try
    {
        nlohmann::json obj = nlohmann::json::parse(f);
        nlohmann::json jmat = obj["floorMat"];
        glm::mat4 mat(jmat[0][0].template get<float>(), jmat[1][0].template get<float>(), jmat[2][0].template get<float>(), jmat[3][0].template get<float>(),
                      jmat[0][1].template get<float>(), jmat[1][1].template get<float>(), jmat[2][1].template get<float>(), jmat[3][1].template get<float>(),
                      jmat[0][2].template get<float>(), jmat[1][2].template get<float>(), jmat[2][2].template get<float>(), jmat[3][2].template get<float>(),
                      jmat[0][3].template get<float>(), jmat[1][3].template get<float>(), jmat[2][3].template get<float>(), jmat[3][3].template get<float>());
        floorMat = mat;
    }
    catch (const nlohmann::json::exception& e)
    {
        std::string s = e.what();
        Log::E("VrConfig::ImportJson exception: %s\n", s.c_str());
        return false;
    }

    return true;
}

bool VrConfig::ExportJson(const std::string& jsonFilename) const
{
    std::ofstream f(jsonFilename);
    if (f.fail())
    {
        return false;
    }

    f << "{" << std::endl;
    f << "    \"floorMat\": [";
    f << "[" << floorMat[0][0] << ", " << floorMat[1][0] << ", " << floorMat[2][0] << ", " << floorMat[3][0] << "], ";
    f << "[" << floorMat[0][1] << ", " << floorMat[1][1] << ", " << floorMat[2][1] << ", " << floorMat[3][1] << "], ";
    f << "[" << floorMat[0][2] << ", " << floorMat[1][2] << ", " << floorMat[2][2] << ", " << floorMat[3][2] << "], ";
    f << "[" << floorMat[0][3] << ", " << floorMat[1][3] << ", " << floorMat[2][3] << ", " << floorMat[3][3] << "]]";
    f << std::endl << "}";

    return true;
}
