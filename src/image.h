#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <string>
#include <vector>

enum class PixelFormat {
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
    std::vector<uint8_t> data;
};

#endif
