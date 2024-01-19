/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

enum class PixelFormat
{
    R = 0,  // intensity
    RA,     // intensity alpha
    RGB,
    RGBA
};

struct Image {
    Image();
    bool Load(const std::string& filename);
    void MultiplyAlpha();

    uint32_t width;
    uint32_t height;
    PixelFormat pixelFormat;
    bool isSRGB;
    std::vector<uint8_t> data;
};
