#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "colour.h"

class PbrMaterial
{
  public:
    PbrMaterial()
        : PbrMaterial(base_colour)
    {
    }

    PbrMaterial(
        const Colour& base_colour)
        : base_colour(base_colour)
    {
    }

    Colour base_colour;
};

#endif