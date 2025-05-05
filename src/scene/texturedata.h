#ifndef SCENE_TEXTUREDATA_H_INCLUDED
#define SCENE_TEXTUREDATA_H_INCLUDED

#include "lalgebra.h"
#include "util.h"

class TextureData
{
  public:
    TextureData();
    TextureData(const std::string& path, bool is_srgb);
    TextureData(const uint8_t* data, size_t data_size, uint32_t width, uint32_t height, VkFormat format);
    TextureData(TextureData&& other);
    ~TextureData();

    Vec4 colour_at(uint32_t x, uint32_t y) const;

    inline uint32_t get_width() const { return width; }
    inline uint32_t get_height() const { return height; }
    inline size_t get_data_size() const { return data_size; }
    inline const void* data_handle() const { return data; }
    bool contains_data();
    VkFormat get_image_format() const { return format; };
    bool has_alpha_less_than_one();

    TextureData& operator=(TextureData&& other);

  private:
    uint8_t* data;
    uint32_t width, height;
    size_t data_size;
    VkFormat format;

    bool loaded_with_stb_image;

    void free_data();

    NO_COPY(TextureData);
};

#endif