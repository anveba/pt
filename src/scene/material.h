#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "colour.h"

class PbrMaterial
{
  public:
    PbrMaterial()
    {
    }

    alignas(16) Vec4 base_colour;
    alignas(16) Vec4 emission;
};

#endif