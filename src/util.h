// helper functions

#ifndef UTIL_H
#define UTIL_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <SDL.h>
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

glm::vec3 RandomColor();

const std::string& GetRootPath();

#endif
