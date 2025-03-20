#include "texturedata.h"
#include <cassert>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

TextureData::TextureData(const std::string& path)
{
    int width, height, channels;
    data = stbi_load("container.jpg", &width, &height, &channels, 0); 
    if (data == NULL)
        throw std::runtime_error("Could not load texture: " + std::string(stbi_failure_reason()));
    this->width = static_cast<uint32_t>(width);
    this->height = static_cast<uint32_t>(height);
    this->channels = static_cast<uint32_t>(channels);
}

TextureData::~TextureData()
{
    stbi_image_free(data);
}
