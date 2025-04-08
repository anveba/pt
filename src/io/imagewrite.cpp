#include "imagewrite.h"

#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stdexcept>

OutputImageFormat image_format_from_string(const std::string& str)
{
    if (str == "png" || str == "PNG")
        return OutputImageFormat::PNG;
    if (str == "hdr" || str == "HDR")
        return OutputImageFormat::HDR;
    else
        return OutputImageFormat::NONE;
}

void write_hdr(const uint8_t* map, const std::string& out_path, uint32_t width, uint32_t height, uint32_t row_pitch)
{
    float* data = new float[width * height * 4];

    for (uint32_t y = 0; y < height; y++) {
        float* row = (float*)map;
        for (uint32_t x = 0; x < width; x++)
            memcpy(data + (y * width + x) * 4, row + x * 4, 4 * sizeof(float));

        map += row_pitch;
    }

    int result = stbi_write_hdr(out_path.c_str(), width, height, 4, data);

    delete[] data;

    if (!result)
        throw std::runtime_error("Could not write HDR file.");
}

uint8_t float_to_byte(float x)
{
    return static_cast<unsigned char>(std::clamp(x * 255.0f, 0.0f, 255.0f));
}

void write_png(const uint8_t* map, const std::string& out_path, uint32_t width, uint32_t height, uint32_t row_pitch)
{
    uint32_t* data = new uint32_t[width * height];

    for (uint32_t y = 0; y < height; y++) {
        float* row = (float*)map;
        for (uint32_t x = 0; x < width; x++) {
            uint8_t* d = (uint8_t*)&data[y * width + x];
            d[0] = float_to_byte(row[x * 4 + 0]);
            d[1] = float_to_byte(row[x * 4 + 1]);
            d[2] = float_to_byte(row[x * 4 + 2]);
            d[3] = float_to_byte(row[x * 4 + 3]);
        }

        map += row_pitch;
    }

    int result = stbi_write_png(out_path.c_str(), width, height, 4, data, 4 * width);

    delete[] data;

    if (!result)
        throw std::runtime_error("Could not write PNG file.");
}