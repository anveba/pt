#ifndef SCENE_TEXTUREDATA_H_INCLUDED
#define SCENE_TEXTUREDATA_H_INCLUDED

#include "lalgebra.h"
#include "util.h"

class TextureData
{
  public:
    TextureData();
    TextureData(const std::string& path);
    TextureData(TextureData&& other);
    ~TextureData();

    void convert_from_srgb();

    Vec4 colour_at(uint32_t x, uint32_t y) const;

    inline uint32_t get_width() const { return width; }
    inline uint32_t get_height() const { return height; }
    inline const void* data_handle() const { return data; }
    bool contains_data();

    TextureData& operator=(TextureData&& other);

  private:
    void* data;
    uint32_t width, height;

    NO_COPY(TextureData);
};

#endif