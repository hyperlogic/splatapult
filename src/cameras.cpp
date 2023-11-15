#include "cameras.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <Eigen/Dense>

#include "core/log.h"
#include "core/util.h"

Cameras::Cameras()
{
    Eigen::MatrixXf A = Eigen::MatrixXf::Random(3, 2);
    std::cout << "Here is the matrix A:\n" << A << std::endl;
    Eigen::VectorXf b = Eigen::VectorXf::Random(3);
    std::cout << "Here is the right hand side b:\n" << b << std::endl;
    std::cout << "The least-squares solution is:\n"
              << A.colPivHouseholderQr().solve(b) << std::endl;
}

bool Cameras::ImportJson(const std::string& jsonFilename)
{
    std::ifstream f(jsonFilename);
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

    EstimateFloorPlane();

    return true;
}

void Cameras::EstimateFloorPlane()
{
    std::vector<glm::vec3> posVec;
    posVec.reserve(cameraVec.size());
    glm::vec3 max(std::numeric_limits<float>::lowest());
    glm::vec3 min(std::numeric_limits<float>::max());
    for (auto&& c : cameraVec)
    {
        glm::vec3 pos(c[3]);
        posVec.push_back(pos);
        max = glm::max(pos, max);
        min = glm::min(pos, min);
    }

    Log::printf("camera pos extents\n");
    PrintVec(max, "    max");
    PrintVec(min, "    min");
    glm::vec3 d = max - min;
    int smallest_dim;
    if (d.x < d.y && d.x < d.z)
    {
        smallest_dim = 0;
    }
    else if (d.y < d.z)
    {
        smallest_dim = 1;
    }
    else
    {
        smallest_dim = 2;
    }
    Log::printf("smallest dimension = %d\n", smallest_dim);

    Eigen::MatrixXf X = Eigen::MatrixXf(posVec.size(), 3);
    Eigen::VectorXf y = Eigen::VectorXf(posVec.size());
    for (size_t i = 0; i < posVec.size(); i++)
    {
        if (smallest_dim == 0)
        {
            X(i, 0) = posVec[i][1];
            X(i, 1) = posVec[i][2];
        }
        else if (smallest_dim == 1)
        {
            X(i, 0) = posVec[i][0];
            X(i, 1) = posVec[i][2];
        }
        else
        {
            X(i, 0) = posVec[i][0];
            X(i, 1) = posVec[i][1];
        }
        X(i, 2) = 1.0f;
        y(i, 0) = posVec[i][smallest_dim];
    }

    Eigen::VectorXf w = X.colPivHouseholderQr().solve(y);
    std::cout << "Here is the design-matrix X:\n" << X << std::endl;
    std::cout << "Here is the y vector:\n" << y << std::endl;
    std::cout << "The least-squares weights are:\n" << w << std::endl;

    glm::vec3 n;
    glm::vec3 p(0.0f, 0.0f, 0.0f);
    if (smallest_dim == 0)
    {
        n = glm::cross(glm::normalize(glm::vec3(-w[0], 1, 0)), glm::normalize(glm::vec3(-w[1], 0, 1)));
    }
    if (smallest_dim == 1)
    {
        n = glm::cross(glm::normalize(glm::vec3(1, -w[0], 0)), glm::normalize(glm::vec3(0, -w[1], 1)));
    }
    if (smallest_dim == 2)
    {
        n = glm::cross(glm::normalize(glm::vec3(1, 0, -w[0])), glm::normalize(glm::vec3(0, 1, -w[1])));
    }
    p[smallest_dim] = glm::dot(glm::vec3(w[0], w[1], w[2]), glm::vec3(0.0f, 0.0f, 1.0f));

    PrintVec(n, "floorNormal");
    PrintVec(p, "floorPos");

    carpetNormal = n;
    carpetPos = p;
}
