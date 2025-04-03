#ifndef MATERIAL_HLSLI_INCLUDED
#define MATERIAL_HLSLI_INCLUDED

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

struct PbrMaterial {
    float4 base_colour;
    float4 emission;
    float4 rough_metal_normal_map_bits;
    float4 specular;
    float4 sheen;
    float4 cc_ccrgh_aniso;
};

uint map_bits(in PbrMaterial material) {
    return asuint(material.rough_metal_normal_map_bits.w);
}

#endif