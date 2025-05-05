#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "constants.h"
#include "lalgebra.h"

static inline uint32_t& asuint(float& f)
{
    return *reinterpret_cast<uint32_t*>(&f);
}

static inline const uint32_t& asuint(const float& f)
{
    return *reinterpret_cast<const uint32_t*>(&f);
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

    inline uint32_t& map_bits() { return asuint(rough_metal_normal_map_bits.w); }
    inline const uint32_t& map_bits() const { return asuint(rough_metal_normal_map_bits.w); }

    bool is_emitter() const;
    inline bool is_transparent() const { return bool(map_bits() & MATERIAL_IS_TRANSPARENT_BIT); }

    uint32_t emission_map_index() const;

    void offset_maps_by(uint32_t tex_offset);
};

#endif