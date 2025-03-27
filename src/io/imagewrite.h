#ifndef IO_IMAGEWRITE_H_INCLUDED
#define IO_IMAGEWRITE_H_INCLUDED

#include <cstdint>
#include <string>

enum class OutputImageFormat
{
    NONE,
    PNG,
    HDR
};

OutputImageFormat image_format_from_string(const std::string& str);

void write_hdr(const uint8_t* map, const std::string& out_path, uint32_t width, uint32_t height, uint32_t row_pitch);

void write_png(const uint8_t* map, const std::string& out_path, uint32_t width, uint32_t height, uint32_t row_pitch);

#endif