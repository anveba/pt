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
    alignas(16) Uint4 col_emi_rgh_spec_maps;
    alignas(16) Uint4 shn_clcoat_metal_norm_maps;

    inline void clear() {
        base_colour = emission = specular = sheen = clearcoat = metalness_anisotropy = Vec4(0.0f);
        col_emi_rgh_spec_maps = shn_clcoat_metal_norm_maps = Uint4(UINT32_MAX);
    }
};

#endif