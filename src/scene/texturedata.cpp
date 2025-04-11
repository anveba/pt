#include "texturedata.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

TextureData::TextureData()
    : data(nullptr)
    , width(0)
    , height(0)
{
}

TextureData::TextureData(const std::string& path)
{
    int x, y, comp;
    data = stbi_load(path.c_str(), &x, &y, &comp, 4); // TODO variable channel counts
    if (data == nullptr)
        throw std::runtime_error("Could not read texture " + path);
    width = static_cast<uint32_t>(x);
    height = static_cast<uint32_t>(y);
}

TextureData::TextureData(TextureData&& other)
    : TextureData()
{
    *this = std::move(other);
}

TextureData::~TextureData()
{
    if (data != nullptr) {
        stbi_image_free(data);
    }
}

static float from_srgb(float x)
{
    return (x <= 0.04045f) ? (x / 12.92f) : std::pow((x + 0.055f) / 1.055f, 2.4f);
}

static uint8_t from_srgb(uint8_t x)
{
    float f = from_srgb((float)x / 255.0f);
    return (uint8_t)std::max(std::min(f * 255.0f, 255.0f), 0.0f);
}

void TextureData::convert_from_srgb()
{
    // TODO turns out this is quite slow
    for (size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
        uint8_t* data_as_bytes = (uint8_t*)data;
        data_as_bytes[i * 4 + 0] = from_srgb(data_as_bytes[i * 4 + 0]);
        data_as_bytes[i * 4 + 1] = from_srgb(data_as_bytes[i * 4 + 1]);
        data_as_bytes[i * 4 + 2] = from_srgb(data_as_bytes[i * 4 + 2]);
    }
}

Vec4 TextureData::colour_at(uint32_t x, uint32_t y) const
{
    assert(x < get_width());
    assert(y < get_height());
    uint8_t* data_as_bytes = (uint8_t*)data;
    return Vec4(
        data_as_bytes[(y * get_width() + x) * 4 + 0] / 255.0f,
        data_as_bytes[(y * get_width() + x) * 4 + 1] / 255.0f,
        data_as_bytes[(y * get_width() + x) * 4 + 2] / 255.0f,
        data_as_bytes[(y * get_width() + x) * 4 + 3] / 255.0f);
}

bool TextureData::contains_data()
{
    if (data == nullptr) {
        assert(width == 0);
        assert(height == 0);
        return false;
    }
    assert(width > 0);
    assert(height > 0);
    return true;
}

TextureData& TextureData::operator=(TextureData&& other)
{
    if (data)
        stbi_image_free(data);
    data = other.data;
    width = other.width;
    height = other.height;

    other.data = nullptr;
    other.width = other.height = 0;

    return *this;
}
