#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>

struct Image;

enum class FilterType {
    Nearest = 0,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear
};

enum class WrapType {
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge,
    MirrorClampToEdge
};

struct Texture
{
    struct Params {
        FilterType minFilter;
        FilterType magFilter;
        WrapType sWrap;
        WrapType tWrap;
    };

    Texture(const Image& image, const Params& params);
    ~Texture();

    void Apply(int unit) const;

    uint32_t texture;
    bool hasAlphaChannel;
};

#endif
