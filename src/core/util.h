/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

// helper functions

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

// returns true on success, false on failure
bool LoadFile(const std::string& filename, std::string& result);
bool SaveFile(const std::string& filename, const std::string& data);

// Iterate over codepoints in a utf-8 encoded string
int NextCodePointUTF8(const char *str, uint32_t *codePointOut);

#ifndef NDEBUG
#define GL_ERROR_CHECK(x) GLErrorCheck(x)
void GLErrorCheck(const char* message);
#else
#define GL_ERROR_CHECK(x)
#endif

glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& ifZero);
glm::quat SafeMix(const glm::quat& a, const glm::quat& b, float alpha);

glm::mat3 MakeMat3(const glm::quat& rotation);
glm::mat3 MakeMat3(const glm::vec3& scale, const glm::quat& rotation);
glm::mat4 MakeMat3(float scale, const glm::quat& rotation);

glm::mat4 MakeMat4(const glm::vec3& scale, const glm::quat& rotation, const glm::vec3& translation);
glm::mat4 MakeMat4(float scale, const glm::quat& rotation, const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::quat& rotation, const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::quat& rotation);
glm::mat4 MakeMat4(const glm::mat3& m3, const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::mat3& m3);

void Decompose(const glm::mat4& matrix, glm::vec3* scaleOut, glm::quat* rotationOut, glm::vec3* translationOut);
void Decompose(const glm::mat3& matrix, glm::vec3* scaleOut, glm::quat* rotationOut);
void DecomposeSwingTwist(const glm::quat& rotation, const glm::vec3& twistAxis, glm::quat* swingOut, glm::quat* twistOut);

glm::vec3 XformPoint(const glm::mat4& m, const glm::vec3& p);
glm::vec3 XformVec(const glm::mat4& m, const glm::vec3& v);

glm::vec3 RandomColor();

void PrintMat(const glm::mat4& m4, const std::string& name);
void PrintMat(const glm::mat3& m3, const std::string& name);
void PrintMat(const glm::mat2& m2, const std::string& name);
void PrintVec(const glm::vec4& v4, const std::string& name);
void PrintVec(const glm::vec3& v3, const std::string& name);
void PrintVec(const glm::vec2& v2, const std::string& name);
void PrintQuat(const glm::quat& q, const std::string& name);

const std::string& GetRootPath();
void SetRootPath(const std::string& rootPathIn);

bool PointInsideAABB(const glm::vec3& point, const glm::vec3& aabbMin, const glm::vec3& aabbMax);
float LinearToSRGB(float linear);
float SRGBToLinear(float srgb);
glm::vec4 LinearToSRGB(const glm::vec4& linearColor);
glm::vec4 SRGBToLinear(const glm::vec4& srgbColor);

glm::mat4 MakeRotateAboutPointMat(const glm::vec3& pos, const glm::quat& rot);

enum GraphicsAPI { GRAPHICS_VULKAN, GRAPHICS_OPENGL, GRAPHICS_OPENGL_ES, GRAPHICS_D3D };
void CreateProjection(float* m, GraphicsAPI graphicsApi, const float tanAngleLeft,
                      const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
                      const float nearZ, const float farZ);

void StrCpy_s(char* dest, size_t destsz, const char* src);
