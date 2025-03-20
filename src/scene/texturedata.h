#ifndef SCENE_TEXTUREDATA_H_INCLUDED
#define SCENE_TEXTUREDATA_H_INCLUDED

#include "util.h"

class TextureData
{
public:
    TextureData(const std::string& path);
    ~TextureData();

    inline uint32_t get_width() const { return width; }
    inline uint32_t get_height() const { return height; }
    inline uint32_t number_of_channels() const { return channels; }
    inline const uint8_t* get_data() const { return data; }

private:
    uint32_t width, height, channels;
    uint8_t *data;

    NO_COPY(TextureData);
};

#endif