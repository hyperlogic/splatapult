#include "util.h"

#include <cassert>
#include <fstream>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "log.h"

bool LoadFile(const std::string& filename, std::string& data)
{
    std::ifstream ifs(GetRootPath() + filename, std::ifstream::in);
    if (ifs.good())
    {
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        data = std::move(content);
        return true;
    }
    else
    {
        return false;
    }
}

bool SaveFile(const std::string& filename, const std::string& data)
{
    std::ofstream ofs(GetRootPath() + filename, std::ofstream::out);
    if (ofs.good())
    {
        ofs << data;
        return true;
    }
    else
    {
        return false;
    }
}

// returns the number of bytes to advance
// fills cp_out with the code point at p.
int NextCodePointUTF8(const char *str, uint32_t *codePointOut)
{
    const uint8_t* p = (const uint8_t*)str;
    if ((*p & 0x80) == 0)
    {
        *codePointOut = *p;
        return 1;
    }
    else if ((*p & 0xe0) == 0xc0) // 110xxxxx 10xxxxxx
    {
        *codePointOut = ((*p & ~0xe0) << 6) | (*(p+1) & ~0xc0);
        return 2;
    }
    else if ((*p & 0xf0) == 0xe0) // 1110xxxx 10xxxxxx 10xxxxxx
    {
        *codePointOut = ((*p & ~0xf0) << 12) | ((*(p+1) & ~0xc0) << 6) | (*(p+2) & ~0xc0);
        return 3;
    }
    else if ((*p & 0xf8) == 0xf0) // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    {
        *codePointOut = ((*p & ~0xf8) << 18) | ((*(p+1) & ~0xc0) << 12) | ((*(p+1) & ~0xc0) << 6) | (*(p+2) & ~0xc0);
        return 4;
    }
    else
    {
        // p is not at a valid starting point. p is not utf8 encoded or is at a bad offset.
        assert(0);
        *codePointOut = 0;
        return 1;
    }
}

#ifndef NDEBUG
// If there is a glError this outputs it along with a message to stderr.
// otherwise there is no output.
void GLErrorCheck(const char* message)
{
    GLenum val = glGetError();
    switch (val)
    {
    case GL_INVALID_ENUM:
        Log::printf("GL_INVALID_ENUM : %s\n", message);
        break;
    case GL_INVALID_VALUE:
        Log::printf("GL_INVALID_VALUE : %s\n", message);
        break;
    case GL_INVALID_OPERATION:
        Log::printf("GL_INVALID_OPERATION : %s\n", message);
        break;
#ifndef GL_ES_VERSION_2_0
    case GL_STACK_OVERFLOW:
        Log::printf("GL_STACK_OVERFLOW : %s\n", message);
        break;
    case GL_STACK_UNDERFLOW:
        Log::printf("GL_STACK_UNDERFLOW : %s\n", message);
        break;
#endif
    case GL_OUT_OF_MEMORY:
        Log::printf("GL_OUT_OF_MEMORY : %s\n", message);
        break;
    case GL_NO_ERROR:
        break;
    }
}
#endif

glm::quat SafeMix(const glm::quat& a, const glm::quat& b, float alpha)
{
    // adjust signs if necessary
    glm::quat bTemp = b;
    float dot = glm::dot(a, bTemp);
    if (dot < 0.0f)
    {
        bTemp = -bTemp;
    }
    return glm::normalize(glm::lerp(a, bTemp, alpha));
}

glm::mat3 MakeMat3(const glm::quat& rotation)
{
    glm::vec3 xAxis = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 yAxis = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 zAxis = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    return glm::mat3(xAxis, yAxis, zAxis);
}

glm::mat3 MakeMat3(const glm::vec3& scale, const glm::quat& rotation)
{
    glm::vec3 xAxis = rotation * glm::vec3(scale.x, 0.0f, 0.0f);
    glm::vec3 yAxis = rotation * glm::vec3(0.0f, scale.y, 0.0f);
    glm::vec3 zAxis = rotation * glm::vec3(0.0f, 0.0f, scale.z);
    return glm::mat3(xAxis, yAxis, zAxis);
}

glm::mat4 MakeMat3(float scale, const glm::quat& rotation)
{
    return MakeMat3(glm::vec3(scale), rotation);
}

glm::mat4 MakeMat4(const glm::vec3& scale, const glm::quat& rotation, const glm::vec3& translation)
{
    glm::vec3 xAxis = rotation * glm::vec3(scale.x, 0.0f, 0.0f);
    glm::vec3 yAxis = rotation * glm::vec3(0.0f, scale.y, 0.0f);
    glm::vec3 zAxis = rotation * glm::vec3(0.0f, 0.0f, scale.z);
    return glm::mat4(glm::vec4(xAxis, 0.0f), glm::vec4(yAxis, 0.0f),
                     glm::vec4(zAxis, 0.0f), glm::vec4(translation, 1.0f));
}

glm::mat4 MakeMat4(float scale, const glm::quat& rotation, const glm::vec3& translation)
{
    return MakeMat4(glm::vec3(scale), rotation, translation);
}

glm::mat4 MakeMat4(const glm::quat& rotation, const glm::vec3& translation)
{
    return MakeMat4(glm::vec3(1.0f), rotation, translation);
}

glm::mat4 MakeMat4(const glm::quat& rotation)
{
    return MakeMat4(glm::vec3(1.0f), rotation, glm::vec3(0.0f));
}

glm::mat4 MakeMat4(const glm::mat3& m3, const glm::vec3& translation)
{
    return glm::mat4(glm::vec4(m3[0], 0.0f), glm::vec4(m3[1], 0.0f),
                     glm::vec4(m3[2], 0.0f), glm::vec4(translation, 1.0f));
}

glm::mat4 MakeMat4(const glm::mat3& m3)
{
    return MakeMat4(m3, glm::vec3(0.0f));
}

void Decompose(const glm::mat4& matrix, glm::vec3* scaleOut, glm::quat* rotationOut, glm::vec3* translationOut)
{
    glm::mat3 m(matrix);
    float det = glm::determinant(m);
    if (det < 0.0f)
    {
        // left handed matrix, flip sign to compensate.
        *scaleOut = glm::vec3(-glm::length(m[0]), glm::length(m[1]), glm::length(m[2]));
    }
    else
    {
        *scaleOut = glm::vec3(glm::length(m[0]), glm::length(m[1]), glm::length(m[2]));
    }

    // quat_cast doesn't work so well with scaled matrices, so cancel it out.
    glm::mat4 tmp = glm::scale(matrix, 1.0f / *scaleOut);
    *rotationOut = glm::normalize(glm::quat_cast(tmp));

    *translationOut = glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
}

void Decompose(const glm::mat3& matrix, glm::vec3* scaleOut, glm::quat* rotationOut)
{
    float det = glm::determinant(matrix);
    if (det < 0.0f)
    {
        // left handed matrix, flip sign to compensate.
        *scaleOut = glm::vec3(-glm::length(matrix[0]), glm::length(matrix[1]), glm::length(matrix[2]));
    }
    else
    {
        *scaleOut = glm::vec3(glm::length(matrix[0]), glm::length(matrix[1]), glm::length(matrix[2]));
    }

    // quat_cast doesn't work so well with scaled matrices, so cancel it out.
    glm::mat3 tmp;
    tmp[0] = matrix[0] * 1.0f / scaleOut->x;
    tmp[1] = matrix[1] * 1.0f / scaleOut->y;
    tmp[2] = matrix[2] * 1.0f / scaleOut->z;
    *rotationOut = glm::normalize(glm::quat_cast(tmp));
}

void DecomposeSwingTwist(const glm::quat& rotation, const glm::vec3& twistAxis, glm::quat* swingOut, glm::quat* twistOut)
{
    glm::vec3 d = glm::normalize(twistAxis);

    // the twist part has an axis (imaginary component) that is parallel to twistAxis argument
    glm::vec3 axisOfRotation(rotation.x, rotation.y, rotation.z);
    glm::vec3 twistImaginaryPart = glm::dot(d, axisOfRotation) * d;

    // and a real component that is relatively proportional to rotation's real component
    *twistOut = glm::normalize(glm::quat(rotation.w, twistImaginaryPart.x, twistImaginaryPart.y, twistImaginaryPart.z));

    // once twist is known we can solve for swing:
    // rotation = swing * twist  -->  swing = rotation * invTwist
    *swingOut = rotation * glm::inverse(*twistOut);
}

glm::vec3 XformPoint(const glm::mat4& m, const glm::vec3& p)
{
    glm::vec4 temp(p, 1.0f);
    glm::vec4 result = m * temp;
    return glm::vec3(result.x / result.w, result.y / result.w, result.z / result.w);
}

glm::vec3 RandomColor()
{
    return glm::vec3(glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f));
}

void PrintMat(const glm::mat4& m4, const std::string& name)
{
    Log::printf("%s =\n", name.c_str());
    Log::printf("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][0], m4[1][0], m4[2][0], m4[3][0]);
    Log::printf("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][1], m4[1][1], m4[2][1], m4[3][1]);
    Log::printf("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][2], m4[1][2], m4[2][2], m4[3][2]);
    Log::printf("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][3], m4[1][3], m4[2][3], m4[3][3]);
}

void PrintMat(const glm::mat3& m3, const std::string& name)
{
    Log::printf("%s =\n", name.c_str());
    Log::printf("   | %10.5f, %10.5f, %10.5f |\n", m3[0][0], m3[1][0], m3[2][0]);
    Log::printf("   | %10.5f, %10.5f, %10.5f |\n", m3[0][1], m3[1][1], m3[2][1]);
    Log::printf("   | %10.5f, %10.5f, %10.5f |\n", m3[0][2], m3[1][2], m3[2][2]);
}

void PrintMat(const glm::mat2& m2, const std::string& name)
{
    Log::printf("%s =\n", name.c_str());
    Log::printf("   | %10.5f, %10.5f |\n", m2[0][0], m2[1][0]);
    Log::printf("   | %10.5f, %10.5f |\n", m2[0][1], m2[1][1]);
}

void PrintVec(const glm::vec4& v4, const std::string& name)
{
    Log::printf("%s = ( %.5f, %.5f, %.5f, %.5f )\n", name.c_str(), v4.x, v4.y, v4.z, v4.w);
}

void PrintVec(const glm::vec3& v3, const std::string& name)
{
    Log::printf("%s = ( %.5f, %.5f, %.5f )\n", name.c_str(), v3.x, v3.y, v3.z);
}

void PrintVec(const glm::vec2& v2, const std::string& name)
{
    Log::printf("%s = ( %.5f, %.5f )\n", name.c_str(), v2.x, v2.y);
}

static const std::string ROOT_PATH("../../");
const std::string& GetRootPath()
{
    return ROOT_PATH;
}

bool PointInsideAABB(const glm::vec3& point, const glm::vec3& aabbMin, const glm::vec3& aabbMax)
{
    return (point.x >= aabbMin.x && point.x <= aabbMax.x) &&
           (point.y >= aabbMin.y && point.y <= aabbMax.y) &&
           (point.z >= aabbMin.z && point.z <= aabbMax.z);
}
