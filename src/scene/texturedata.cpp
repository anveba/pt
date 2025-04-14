#include "texturedata.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <dds_image/dds.hpp>

static VkFormat get_vulkan_format_from_image_info(uint32_t channels, bool is_srgb)
{
    switch (channels) {
        case 1:
            return is_srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
        case 2:
            return is_srgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
        case 4:
            return is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        default:
            throw std::runtime_error("Invalid number of channels.");
    }
}

static VkFormat get_vulkan_format_from_dx(DXGI_FORMAT format, uint32_t& channel_count, int& compression_scheme)
{
    compression_scheme = -1;
    switch (format) {
        case DXGI_FORMAT_BC1_UNORM:
            compression_scheme = 1;
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            compression_scheme = 1;
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_SRGB;

        case DXGI_FORMAT_BC3_UNORM:
            compression_scheme = 3;
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            compression_scheme = 3;
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_SRGB;

        case DXGI_FORMAT_BC5_UNORM:
            compression_scheme = 5;
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_UNORM;

        case DXGI_FORMAT_R8_UNORM:
            channel_count = 1;
            return VK_FORMAT_R8_UNORM;

        case DXGI_FORMAT_R8G8_UNORM:
            channel_count = 2;
            return VK_FORMAT_R8G8_UNORM;

        case DXGI_FORMAT_R8G8B8A8_UNORM:
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            channel_count = 4;
            return VK_FORMAT_R8G8B8A8_SRGB;

        default:
            throw std::runtime_error("Unsupported format: " + std::to_string(format));
    }
}

static void decompress_bc1(uint8_t* dest, uint8_t* source, uint32_t width, uint32_t height, bool has_alpha)
{
    constexpr size_t compressed_block_size = 8;
    constexpr uint16_t red_mask = 0b11111'000000'00000;
    constexpr int red_shift = 11;
    constexpr int red_offset = 3;
    constexpr uint16_t green_mask = 0b00000'111111'00000;
    constexpr int green_shift = 5;
    constexpr int green_offset = 2;
    constexpr uint16_t blue_mask = 0b00000'000000'11111;
    constexpr int blue_shift = 0;
    constexpr int blue_offset = 3;

    assert((width * height) % (4 * 4) == 0);
    const size_t block_count = (width * height) / (4 * 4);

    for (size_t i = 0; i < block_count; i++) {

        uint16_t colour0_raw = *((uint16_t*)&source[i * compressed_block_size + 0]);
        uint16_t colour1_raw = *((uint16_t*)&source[i * compressed_block_size + 2]);

        Uint4 colours[4];
        colours[0] = Uint4(((colour0_raw & red_mask) >> red_shift) << red_offset,
                           ((colour0_raw & green_mask) >> green_shift) << green_offset,
                           ((colour0_raw & blue_mask) >> blue_shift) << blue_offset,
                           255);
        colours[1] = Uint4(((colour1_raw & red_mask) >> red_shift) << red_offset,
                           ((colour1_raw & green_mask) >> green_shift) << green_offset,
                           ((colour1_raw & blue_mask) >> blue_shift) << blue_offset,
                           255);

        if (has_alpha) {
            colours[3] = Uint4(0, 0, 0, 0);
            colours[2] = glm::min((colours[0] + colours[1]) / 2u, 255u);
        } else {
            colours[2] = glm::min((2u * colours[0] + colours[1]) / 3u, 255u);
            colours[3] = glm::min(((colours[0] + 2u * colours[1])) / 3u, 255u);
        }

        for (size_t y = 0; y < 4; y++) {

            uint8_t row = source[i * compressed_block_size + 4 + y];

            for (size_t x = 0; x < 4; x++) {

                uint8_t colour_idx = (row >> (x * 2)) & 0b00000011;
                size_t block_x = i % (width / 4);
                size_t block_y = i / (width / 4);
                size_t pixel_x = block_x * 4 + x;
                size_t pixel_y = block_y * 4 + y;
                size_t pixel_idx = pixel_y * width + pixel_x;

                for (int j = 0; j < 4; j++) {
                    assert(pixel_idx * 4 + j < width * height * 4);
                    dest[pixel_idx * 4 + j] = static_cast<uint8_t>(colours[colour_idx][j]);
                }
            }
        }
    }
}

static void decompress(
    int scheme,
    uint8_t* dest,
    uint8_t* source,
    uint32_t width,
    uint32_t height,
    uint32_t channel_count,
    bool has_alpha)
{
    std::cout << "Compression scheme: " << scheme << std::endl;
    if (scheme < 0) {
        memcpy(dest, source, width * height * channel_count);
    } else if (scheme == 1) {
        assert(channel_count == 4);
        decompress_bc1(dest, source, width, height, has_alpha);
    } else if (scheme == 3) {
        assert(channel_count == 4);
        memset(dest, 0xFF00FFFF, width * height * channel_count);
    } else if (scheme == 5) {
        assert(channel_count == 4);
        memset(dest, 0xFF00FFFF, width * height * channel_count);
    } else
        throw std::runtime_error("Unsupported compression scheme.");
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
    if (!stbi_info(path.c_str(), &x, &y, &comp)) {

        dds::Image image;
        if (dds::readFile(path, &image) != dds::ReadResult::Success)
            throw std::runtime_error("Failed to read texture " + path);

        loaded_with_stb_image = false;
        width = image.width;
        height = image.height;

        // int compression_scheme;
        // format = get_vulkan_format_from_dx(image.format, channel_count, compression_scheme);

        // data_size = width * height * channel_count;
        data_size = image.mipmaps.front().size();
        data = new uint8_t[data_size];
        memcpy(data, image.mipmaps.front().data(), data_size);
        format = dds::getVulkanFormat(image.format, image.supportsAlpha);
        channel_count = 4; // TODO

        // decompress(compression_scheme, data, image.mipmaps.front().data(), width, height, channel_count, image.supportsAlpha);

    } else {

        channel_count = static_cast<uint32_t>(comp);
        channel_count = (channel_count == 3) ? 4 : channel_count;
        format = get_vulkan_format_from_image_info(channel_count, is_srgb);

        data = stbi_load(path.c_str(), &x, &y, &comp, channel_count);
        if (data == NULL)
            throw std::runtime_error("Failed to read texture " + path);

        loaded_with_stb_image = true;
        width = static_cast<uint32_t>(x);
        height = static_cast<uint32_t>(y);
        data_size = static_cast<size_t>(width) * height * channel_count;
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
    if (get_image_format() == VK_FORMAT_R8G8B8A8_UNORM) {
        assert(x < get_width());
        assert(y < get_height());
        return Vec4(
            data[(y * get_width() + x) * 4 + 0] / 255.0f,
            data[(y * get_width() + x) * 4 + 1] / 255.0f,
            data[(y * get_width() + x) * 4 + 2] / 255.0f,
            data[(y * get_width() + x) * 4 + 3] / 255.0f);
    } else {
        return Vec4(0.5f);
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
    channel_count = other.channel_count;
    data_size = other.data_size;
    format = other.format;
    loaded_with_stb_image = other.loaded_with_stb_image;

    other.data = nullptr;
    other.width = other.height = other.channel_count = other.data_size = 0;
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
