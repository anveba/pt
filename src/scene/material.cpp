#include "material.h"

bool PbrMaterial::is_emitter() const
{
    return ((map_bits() & EMISSION_MAP_BIT) ||
            std::max(std::max(emission.r, emission.g), emission.b) > 0.0f) &&
           emission.a > 0.0f;
}

uint32_t PbrMaterial::emission_map_index() const
{
    assert(map_bits() & EMISSION_MAP_BIT);
    return asuint(emission.r);
}

void PbrMaterial::offset_maps_by(uint32_t tex_offset)
{
    if (map_bits() & BASE_COLOUR_MAP_BIT)
        asuint(base_colour.x) += tex_offset;
    if (map_bits() & EMISSION_MAP_BIT)
        asuint(emission.x) += tex_offset;
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
