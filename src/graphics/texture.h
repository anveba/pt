#ifndef GRAPHICS_TEXTURE_H_INCLUDED
#define GRAPHICS_TEXTURE_H_INCLUDED

#include "util.h"

class Texture
{
  public:
    Texture();
    ~Texture();

  private:
    VkImage image;

    NO_COPY(Texture);
};

#endif