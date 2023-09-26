#include "texture.h"

#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include "image.h"

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
    GL_MIRROR_CLAMP_TO_EDGE
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

Texture::~Texture()
{
    glDeleteTextures(1, &texture);
}

void Texture::Apply(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}
