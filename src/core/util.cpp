/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "util.h"

#include <cassert>
#include <fstream>
#include <string.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#ifndef __ANDROID__
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif

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
        Log::D("GL_INVALID_ENUM : %s\n", message);
        break;
    case GL_INVALID_VALUE:
        Log::D("GL_INVALID_VALUE : %s\n", message);
        break;
    case GL_INVALID_OPERATION:
        Log::D("GL_INVALID_OPERATION : %s\n", message);
        break;
#ifndef GL_ES_VERSION_2_0
    case GL_STACK_OVERFLOW:
        Log::D("GL_STACK_OVERFLOW : %s\n", message);
        break;
    case GL_STACK_UNDERFLOW:
        Log::D("GL_STACK_UNDERFLOW : %s\n", message);
        break;
#endif
    case GL_OUT_OF_MEMORY:
        Log::D("GL_OUT_OF_MEMORY : %s\n", message);
        break;
    case GL_NO_ERROR:
        break;
    }
}
#endif

glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& ifZero)
{
    float len = glm::length(v);
    if (len > 0.0f)
    {
        return glm::normalize(v);
    }
    else
    {
        return ifZero;
    }
}

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

glm::vec3 XformVec(const glm::mat4& m, const glm::vec3& v)
{
    return glm::mat3(m) * v;
}

glm::vec3 RandomColor()
{
    return glm::vec3(glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f));
}

void PrintMat(const glm::mat4& m4, const std::string& name)
{
    Log::D("%s =\n", name.c_str());
    Log::D("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][0], m4[1][0], m4[2][0], m4[3][0]);
    Log::D("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][1], m4[1][1], m4[2][1], m4[3][1]);
    Log::D("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][2], m4[1][2], m4[2][2], m4[3][2]);
    Log::D("   | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][3], m4[1][3], m4[2][3], m4[3][3]);
}

void PrintMat(const glm::mat3& m3, const std::string& name)
{
    Log::D("%s =\n", name.c_str());
    Log::D("   | %10.5f, %10.5f, %10.5f |\n", m3[0][0], m3[1][0], m3[2][0]);
    Log::D("   | %10.5f, %10.5f, %10.5f |\n", m3[0][1], m3[1][1], m3[2][1]);
    Log::D("   | %10.5f, %10.5f, %10.5f |\n", m3[0][2], m3[1][2], m3[2][2]);
}

void PrintMat(const glm::mat2& m2, const std::string& name)
{
    Log::D("%s =\n", name.c_str());
    Log::D("   | %10.5f, %10.5f |\n", m2[0][0], m2[1][0]);
    Log::D("   | %10.5f, %10.5f |\n", m2[0][1], m2[1][1]);
}

void PrintVec(const glm::vec4& v4, const std::string& name)
{
    Log::D("%s = ( %.5f, %.5f, %.5f, %.5f )\n", name.c_str(), v4.x, v4.y, v4.z, v4.w);
}

void PrintVec(const glm::vec3& v3, const std::string& name)
{
    Log::D("%s = ( %.5f, %.5f, %.5f )\n", name.c_str(), v3.x, v3.y, v3.z);
}

void PrintVec(const glm::vec2& v2, const std::string& name)
{
    Log::D("%s = ( %.5f, %.5f )\n", name.c_str(), v2.x, v2.y);
}

void PrintQuat(const glm::quat& q, const std::string& name)
{
    Log::D("%s = ( %.5f, ( %.5f, %.5f, %.5f ) )\n", name.c_str(), q.x, q.y, q.z, q.w);
}

#ifdef SHIPPING
static std::string rootPath("");
#else
#ifdef __linux__
// enables us to run from the build dir
static std::string rootPath("../");
#else
// enables us to run from the build/Debug dir
static std::string rootPath("../../");
#endif
#endif

const std::string& GetRootPath()
{
    return rootPath;
}

void SetRootPath(const std::string& rootPathIn)
{
    rootPath = rootPathIn;
}

bool PointInsideAABB(const glm::vec3& point, const glm::vec3& aabbMin, const glm::vec3& aabbMax)
{
    return (point.x >= aabbMin.x && point.x <= aabbMax.x) &&
           (point.y >= aabbMin.y && point.y <= aabbMax.y) &&
           (point.z >= aabbMin.z && point.z <= aabbMax.z);
}

float LinearToSRGB(float linear)
{
    if (linear <= 0.0031308f)
    {
        return 12.92f * linear;
    }
    else
    {
        return 1.055f * glm::pow(linear, 1.0f / 2.4f) - 0.055f;
    }
}

float SRGBToLinear(float srgb)
{
    if (srgb <= 0.04045f)
    {
        return srgb / 12.92f;
    }
    else
    {
        return glm::pow((srgb + 0.055f) / 1.055f, 2.4f);
    }
}

glm::vec4 LinearToSRGB(const glm::vec4& linearColor)
{
    glm::vec4 sRGBColor;

    for (int i = 0; i < 3; ++i)
    {
        sRGBColor[i] = LinearToSRGB(linearColor[i]);
    }
    sRGBColor.a = linearColor.a;
    return sRGBColor;
}

glm::vec4 SRGBToLinear(const glm::vec4& srgbColor)
{
    glm::vec4 linearColor;
    for (int i = 0; i < 3; ++i) // Convert RGB, leave A unchanged
    {
        linearColor[i] = SRGBToLinear(srgbColor[i]);
    }
    linearColor.a = srgbColor.a; // Copy alpha channel directly
    return linearColor;
}

glm::mat4 MakeRotateAboutPointMat(const glm::vec3& pos, const glm::quat& rot)
{
    glm::mat4 posMat = MakeMat4(glm::quat(), pos);
    glm::mat4 invPosMat = MakeMat4(glm::quat(), -pos);
    glm::mat4 rotMat = MakeMat4(rot);
    return posMat * rotMat * invPosMat;
}

// Creates a projection matrix based on the specified dimensions.
// The projection matrix transforms -Z=forward, +Y=up, +X=right to the appropriate clip space for the graphics API.
// The far plane is placed at infinity if farZ <= nearZ.
// An infinite projection matrix is preferred for rasterization because, except for
// things *right* up against the near plane, it always provides better precision:
//              "Tightening the Precision of Perspective Rendering"
//              Paul Upchurch, Mathieu Desbrun
//              Journal of Graphics Tools, Volume 16, Issue 1, 2012
void CreateProjection(float* m, GraphicsAPI graphicsApi, const float tanAngleLeft,
                      const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
                      const float nearZ, const float farZ)
{
    const float tanAngleWidth = tanAngleRight - tanAngleLeft;

    // Set to tanAngleDown - tanAngleUp for a clip space with positive Y down (Vulkan).
    // Set to tanAngleUp - tanAngleDown for a clip space with positive Y up (OpenGL / D3D / Metal).
    const float tanAngleHeight = graphicsApi == GRAPHICS_VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);

    // Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
    // Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
    const float offsetZ = (graphicsApi == GRAPHICS_OPENGL || graphicsApi == GRAPHICS_OPENGL_ES) ? nearZ : 0;

    if (farZ <= nearZ)
    {
        // place the far plane at infinity
        m[0] = 2 / tanAngleWidth;
        m[4] = 0;
        m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        m[12] = 0;

        m[1] = 0;
        m[5] = 2 / tanAngleHeight;
        m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        m[13] = 0;

        m[2] = 0;
        m[6] = 0;
        m[10] = -1;
        m[14] = -(nearZ + offsetZ);

        m[3] = 0;
        m[7] = 0;
        m[11] = -1;
        m[15] = 0;
    }
    else
    {
        // normal projection
        m[0] = 2 / tanAngleWidth;
        m[4] = 0;
        m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
        m[12] = 0;

        m[1] = 0;
        m[5] = 2 / tanAngleHeight;
        m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
        m[13] = 0;

        m[2] = 0;
        m[6] = 0;
        m[10] = -(farZ + offsetZ) / (farZ - nearZ);
        m[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

        m[3] = 0;
        m[7] = 0;
        m[11] = -1;
        m[15] = 0;
    }
}

void StrCpy_s(char* dest, size_t destsz, const char* src)
{
#ifdef WIN32
    strcpy_s(dest, destsz, src);
#else
    strcpy(dest, src);
#endif
}
