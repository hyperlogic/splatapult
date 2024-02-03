/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "texture.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif

#include "core/image.h"

static GLenum filterTypeToGL[] = {
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

static GLenum wrapTypeToGL[] = {
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
#ifndef __ANDROID__
    GL_MIRROR_CLAMP_TO_EDGE,
#endif
};

static GLenum pixelFormatToGL[] = {
    GL_LUMINANCE,
    GL_LUMINANCE_ALPHA,
    GL_RGB,
    GL_RGBA
};

Texture::Texture(const Image& image, const Params& params)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterTypeToGL[(int)params.minFilter]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterTypeToGL[(int)params.magFilter]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapTypeToGL[(int)params.sWrap]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapTypeToGL[(int)params.tWrap]);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum pf = pixelFormatToGL[(int)image.pixelFormat];

    int internalFormat = pf;

    if (image.isSRGB && pf == GL_RGB)
    {
        internalFormat = GL_SRGB8;
    }
    else if (image.isSRGB && pf == GL_RGBA)
    {
        internalFormat = GL_SRGB8_ALPHA8;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, pf, GL_UNSIGNED_BYTE, &image.data[0]);

    if ((int)params.minFilter >= (int)FilterType::NearestMipmapNearest)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    if (image.pixelFormat == PixelFormat::RA || image.pixelFormat == PixelFormat::RGBA)
    {
        hasAlphaChannel = true;
    }
}

Texture::Texture(uint32_t width, uint32_t height, uint32_t internalFormat,
                 uint32_t format, uint32_t type, const Params& params)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterTypeToGL[(int)params.minFilter]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterTypeToGL[(int)params.magFilter]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapTypeToGL[(int)params.sWrap]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapTypeToGL[(int)params.tWrap]);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
}

Texture::~Texture()
{
    glDeleteTextures(1, &texture);
}

void Texture::Bind(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}
