/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "image.h"
//#include "util.h"
#include "log.h"

#include <string.h>

extern "C" {
#include <png.h>
}

#include "util.h"

Image::Image() : width(0), height(0), pixelFormat(PixelFormat::R), isSRGB(false)
{
}

bool Image::Load(const std::string& filenameIn)
{
    std::string fullFilename = GetRootPath() + filenameIn;
    const char* filename = fullFilename.c_str();

#ifdef _WIN32
    FILE *fp = NULL;
    fopen_s(&fp, filename, "rb");
#else
    FILE *fp = fopen(filename, "rb");
#endif
    if (!fp)
    {
        Log::E("Failed to load texture \"%s\"\n", filename);
        return false;
    }

    unsigned char header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
    {
        Log::E("Texture \"%s\" is not a valid PNG file\n", filename);
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        Log::E("png_create_read_struct() failed\n");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        Log::E("png_create_info_struct() failed\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_byte** row_pointers = png_get_rows(png_ptr, info_ptr);

    png_uint_32 w, h;
    int bit_depth, color_type;
    png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, NULL, NULL, NULL);

    bool loaded = false;
    if (bit_depth != 8)
    {
        Log::E("bad bit depth for texture \"%s\n", filename);
    }
    else
    {
        int pixelSize;
        switch (color_type)
        {
        case PNG_COLOR_TYPE_GRAY:
            pixelFormat = PixelFormat::R;
            pixelSize = 1;
            break;
        case PNG_COLOR_TYPE_GA:
            pixelFormat = PixelFormat::RA;
            pixelSize = 2;
            break;
        case PNG_COLOR_TYPE_RGB:
            pixelFormat = PixelFormat::RGB;
            pixelSize = 3;
            break;
        case PNG_COLOR_TYPE_RGBA:
            pixelFormat = PixelFormat::RGBA;
            pixelSize = 4;
            break;
        default:
            Log::E("unsupported pixel format %d for image \"%s\n", color_type, filename);
            return false;
        }

        width = w;
        height = h;
        data.resize(width * height * pixelSize);

        // copy row pointers into data vector
        for (int i = 0; i < (int)height; ++i)
        {
            memcpy(&data[0] + i * width * pixelSize, row_pointers[height - 1 - i], width * pixelSize);
        }

        // pre-multiply alpha
        MultiplyAlpha();

        loaded = true;
    }

    png_destroy_info_struct(png_ptr, &info_ptr);
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);

    // TODO: should probably check if the sRGB color space chunk is present
    isSRGB = true;

    return loaded;
}

void Image::MultiplyAlpha()
{
    if (pixelFormat == PixelFormat::R || pixelFormat == PixelFormat::RGB)
    {
        return;
    }
    else if (pixelFormat == PixelFormat::RA)
    {
        size_t pixelSize = 2;
        for (size_t i = 0; i < width * height; i++)
        {
            float red = (float)data[i * pixelSize] / 255.0f;
            float alpha = (float)data[i * pixelSize + 1] / 255.0f;
            data[i * pixelSize] = (uint8_t)((red * alpha) * 255.0f);
        }
    }
    else if (pixelFormat == PixelFormat::RGBA)
    {
        size_t pixelSize = 4;
        for (size_t i = 0; i < width * height; i++)
        {
            float red = (float)data[i * pixelSize] / 255.0f;
            float green = (float)data[i * pixelSize + 1] / 255.0f;
            float blue = (float)data[i * pixelSize + 2] / 255.0f;
            float alpha = (float)data[i * pixelSize + 3] / 255.0f;
            data[i * pixelSize] = (uint8_t)((red * alpha) * 255.0f);
            data[i * pixelSize + 1] = (uint8_t)((green * alpha) * 255.0f);
            data[i * pixelSize + 2] = (uint8_t)((blue * alpha) * 255.0f);
        }
    }
}
