/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>

struct Image;

enum class FilterType
{
    Nearest = 0,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear
};

enum class WrapType
{
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge,
    MirrorClampToEdge
};

struct Texture
{
    struct Params
    {
        FilterType minFilter;
        FilterType magFilter;
        WrapType sWrap;
        WrapType tWrap;
    };

    Texture(const Image& image, const Params& params);
    Texture(uint32_t width, uint32_t height, uint32_t internalFormat,
            uint32_t format, uint32_t type, const Params& params);
    ~Texture();

    void Bind(int unit) const;

    uint32_t texture;
    bool hasAlphaChannel;
};
