#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "colour.h"

#define BASE_COLOUR_MAP_BIT (1 << 0)
#define EMISSION_MAP_BIT (1 << 1)
#define EMISSION_INTENSITY_MAP_BIT (1 << 2)
#define ROUGHNESS_MAP_BIT (1 << 3)
#define METALNESS_MAP_BIT (1 << 4)
#define ROUGHNESS_METALNESS_MAP_BIT (1 << 5)
#define SPECULAR_MAP_BIT (1 << 6)
#define SPECULAR_TINT_MAP_BIT (1 << 7)
#define SHEEN_MAP_BIT (1 << 8)
#define SHEEN_TINT_MAP_BIT (1 << 9)
#define CLEARCOAT_MAP_BIT (1 << 10)
#define CLEARCOAT_ROUGHNESS_MAP_BIT (1 << 11)
#define ANISOTROPY_MAP_BIT (1 << 12)
#define NORMAL_MAP_BIT (1 << 13)

static inline uint32_t& asuint(float& f)
{
    return *reinterpret_cast<uint32_t*>(&f);
}

class PbrMaterial
{
  public:
    PbrMaterial()
    {
    }

    alignas(16) Vec4 base_colour;
    alignas(16) Vec4 emission;
    alignas(16) Vec4 rough_metal_normal_map_bits;
    alignas(16) Vec4 specular;
    alignas(16) Vec4 sheen;
    alignas(16) Vec4 cc_ccrgh_aniso;

    inline void clear()
    {
        base_colour = emission = rough_metal_normal_map_bits = specular = sheen = cc_ccrgh_aniso = Vec4(0.0f);
    }

    inline bool is_emitter() const { return std::max(std::max(emission.x, emission.y), emission.z) > 0.0f; }
    inline uint32_t& map_bits() { return asuint(rough_metal_normal_map_bits.w); }

    void offset_maps_by(uint32_t tex_offset)
    {
        if (map_bits() & BASE_COLOUR_MAP_BIT)
            asuint(base_colour.x) += tex_offset;
        if (map_bits() & EMISSION_MAP_BIT)
            asuint(emission.x) += tex_offset;
        if (map_bits() & EMISSION_INTENSITY_MAP_BIT)
            asuint(emission.w) += tex_offset;
        if ((map_bits() & ROUGHNESS_MAP_BIT) || (map_bits() & ROUGHNESS_METALNESS_MAP_BIT))
            asuint(rough_metal_normal_map_bits.x) += tex_offset;
        if (map_bits() & METALNESS_MAP_BIT)
            asuint(rough_metal_normal_map_bits.y) += tex_offset;
        if (map_bits() & SPECULAR_TINT_MAP_BIT)
            asuint(specular.x) += tex_offset;
        if (map_bits() & SPECULAR_MAP_BIT)
            asuint(specular.w) += tex_offset;
        if (map_bits() & SHEEN_TINT_MAP_BIT)
            asuint(sheen.x) += tex_offset;
        if (map_bits() & SHEEN_MAP_BIT)
            asuint(sheen.w) += tex_offset;
        if (map_bits() & CLEARCOAT_MAP_BIT)
            asuint(cc_ccrgh_aniso.x) += tex_offset;
        if (map_bits() & CLEARCOAT_ROUGHNESS_MAP_BIT)
            asuint(cc_ccrgh_aniso.y) += tex_offset;
        if (map_bits() & ANISOTROPY_MAP_BIT)
            asuint(cc_ccrgh_aniso.z) += tex_offset;
        if (map_bits() & NORMAL_MAP_BIT)
            asuint(rough_metal_normal_map_bits.z) += tex_offset;
    }
};

#endif