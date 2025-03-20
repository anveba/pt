#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "colour.h"

class PbrMaterial
{
  public:
    PbrMaterial()
    {
    }

    alignas(16) Vec4 base_colour; // w component is roughness
    alignas(16) Vec4 emission;
    alignas(16) Vec4 specular;
    alignas(16) Vec4 sheen;
    alignas(16) Vec4 clearcoat;
    alignas(16) Vec4 metalness_anisotropy;
    alignas(16) Uint4 base_emission_roughness_specular_maps;
    alignas(16) Uint4 sheen_clearcoat_metalness_normal_maps;
};

#endif