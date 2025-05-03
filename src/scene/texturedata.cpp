#include "texturedata.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

static VkFormat get_vulkan_format_from_image_info(uint32_t channels, bool is_srgb)
{
    switch (channels) {
        case 1:
            return VK_FORMAT_R8_UNORM;
        case 2:
            return VK_FORMAT_R8G8_UNORM;
        case 4:
            return is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        default:
            throw std::runtime_error("Invalid number of channels.");
    }
}

TextureData::TextureData()
    : data(nullptr)
    , width(0)
    , height(0)
    , data_size(0)
    , format(VK_FORMAT_UNDEFINED)
    , loaded_with_stb_image(false)
{
}

TextureData::TextureData(const std::string& path, bool is_srgb)
{
    int x, y, comp;
    if (stbi_info(path.c_str(), &x, &y, &comp)) {
        uint32_t channel_count = static_cast<uint32_t>(comp);
        channel_count = (channel_count == 3) ? 4 : channel_count;
        format = get_vulkan_format_from_image_info(channel_count, is_srgb);

        data = stbi_load(path.c_str(), &x, &y, &comp, channel_count);
        if (data == NULL)
            throw std::runtime_error("Failed to read texture " + path);

        loaded_with_stb_image = true;
        width = static_cast<uint32_t>(x);
        height = static_cast<uint32_t>(y);
        data_size = static_cast<size_t>(width) * height * channel_count;
    } else {
        throw std::runtime_error("Failed to read texture " + path);
    }
}

TextureData::TextureData(const uint8_t* data, size_t data_size, uint32_t width, uint32_t height, VkFormat format)
    : width(width)
    , height(height)
    , format(format)
    , loaded_with_stb_image(false)
{
    this->data = new uint8_t[data_size];
    memcpy(this->data, data, data_size);
}

TextureData::TextureData(const uint8_t* compressed_data, size_t compressed_data_size, bool is_srgb)
{
    int x, y, comp;
    if (stbi_info_from_memory(compressed_data, compressed_data_size, &x, &y, &comp)) {

        uint32_t channel_count = static_cast<uint32_t>(comp);
        channel_count = (channel_count == 3) ? 4 : channel_count;
        format = get_vulkan_format_from_image_info(channel_count, is_srgb);

        data = stbi_load_from_memory(compressed_data, compressed_data_size, &x, &y, &comp, channel_count);
        if (data == NULL)
            throw std::runtime_error("Failed to read texture from memory.");

        loaded_with_stb_image = true;
        width = static_cast<uint32_t>(x);
        height = static_cast<uint32_t>(y);
        data_size = static_cast<size_t>(width) * height * channel_count;
    } else {
        throw std::runtime_error("Failed to read texture from memory.");
    }
}

TextureData::TextureData(TextureData&& other)
    : TextureData()
{
    *this = std::move(other);
}

TextureData::~TextureData()
{
    free_data();
}

Vec4 TextureData::colour_at(uint32_t x, uint32_t y) const
{
    assert(x < get_width());
    assert(y < get_height());
    if (get_image_format() == VK_FORMAT_R8G8B8A8_UNORM || get_image_format() == VK_FORMAT_R8G8B8A8_SRGB) {
        return Vec4(
            data[(y * get_width() + x) * 4 + 0] / 255.0f,
            data[(y * get_width() + x) * 4 + 1] / 255.0f,
            data[(y * get_width() + x) * 4 + 2] / 255.0f,
            data[(y * get_width() + x) * 4 + 3] / 255.0f);
    } else if (get_image_format() == VK_FORMAT_R8G8_UNORM || get_image_format() == VK_FORMAT_R8G8_SRGB) {
        return Vec4(
            data[(y * get_width() + x) * 2 + 0] / 255.0f,
            data[(y * get_width() + x) * 2 + 1] / 255.0f,
            0.0f,
            1.0f);
    } else if (get_image_format() == VK_FORMAT_R8_UNORM || get_image_format() == VK_FORMAT_R8_SRGB) {
        return Vec4(
            data[y * get_width() + x] / 255.0f,
            0.0f,
            0.0f,
            1.0f);
    } else {
        throw std::runtime_error("Texture access for format not supported: " + std::to_string(get_image_format()));
    }
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
    free_data();
    data = other.data;
    width = other.width;
    height = other.height;
    data_size = other.data_size;
    format = other.format;
    loaded_with_stb_image = other.loaded_with_stb_image;

    other.data = nullptr;
    other.width = other.height = other.data_size = 0;
    other.format = VK_FORMAT_UNDEFINED;

    return *this;
}

void TextureData::free_data()
{
    if (data != nullptr) {
        if (loaded_with_stb_image)
            stbi_image_free(data);
        else
            delete[] data;
    }
}
