#ifndef MATERIAL_HLSLI_INCLUDED
#define MATERIAL_HLSLI_INCLUDED

#include "constants.h"

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

PbrMaterial get_material(in StructuredBuffer<PbrMaterial> material_buffer, uint i) {
    return material_buffer[i];
}

float3 get_emission(
    in PbrMaterial material, in float2 uv, 
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler) 
{
    if (map_bits(material) & EMISSION_MAP_BIT)
        return textures[asuint(material.emission.x)].SampleLevel(texture_sampler, uv, 0).rgb * material.emission.a;
    else
        return material.emission.rgb * material.emission.a;
}

float4 get_base_colour(
    in PbrMaterial material, in float2 uv,
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler) 
{
    if (map_bits(material) & BASE_COLOUR_MAP_BIT)
        return textures[asuint(material.base_colour.x)].SampleLevel(texture_sampler, uv, 0);
    else
        return material.base_colour;
}

void get_roughness_metalness_normal(
    in PbrMaterial material, in float2 uv, 
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler,
    out float roughness, out float metalness, out float3 normal) 
{
    if (map_bits(material) & ROUGHNESS_METALNESS_MAP_BIT) {
        float4 t = textures[asuint(material.rough_metal_normal_map_bits.x)].SampleLevel(texture_sampler, uv, 0);
        roughness = t.g;
        metalness = t.b;
    } else {
        if (map_bits(material) & ROUGHNESS_MAP_BIT)
            roughness = textures[asuint(material.rough_metal_normal_map_bits.x)].SampleLevel(texture_sampler, uv, 0).r;
        else
            roughness = material.rough_metal_normal_map_bits.x;

        if (map_bits(material) & METALNESS_MAP_BIT) 
            metalness = textures[asuint(material.rough_metal_normal_map_bits.y)].SampleLevel(texture_sampler, uv, 0).r;
        else
            metalness = material.rough_metal_normal_map_bits.y;
    }

    if (map_bits(material) & NORMAL_MAP_BIT)
        normal = normalize(textures[asuint(material.rough_metal_normal_map_bits.z)].SampleLevel(texture_sampler, uv, 0).xyz * 2.0 - 1.0);
    else
        normal = float3(0.0, 0.0, 1.0);
}

float3 get_specular(
    in PbrMaterial material, in float2 uv,
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler) 
{
    if (map_bits(material) & SPECULAR_MAP_BIT)
        return textures[asuint(material.specular.x)].SampleLevel(texture_sampler, uv, 0).rgb;
    else
        return material.specular.rgb;
}

float3 get_sheen(
    in PbrMaterial material, in float2 uv,
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler) 
{
    if (map_bits(material) & SHEEN_MAP_BIT)
        return textures[asuint(material.sheen.x)].SampleLevel(texture_sampler, uv, 0).rgb;
    else
        return material.sheen.rgb;
}

void get_clearcoat_anisotropy(
    in PbrMaterial material, in float2 uv,
    in Texture2D<float4> textures[MAX_TEXTURE_COUNT], in SamplerState texture_sampler,
    out float clearcoat, out float cc_roughness, out float anisotropy) 
{
    if (map_bits(material) & CLEARCOAT_MAP_BIT)
        clearcoat = textures[asuint(material.cc_ccrgh_aniso.x)].SampleLevel(texture_sampler, uv, 0).r;
    else
        clearcoat = material.cc_ccrgh_aniso.r;

    cc_roughness = material.cc_ccrgh_aniso.g;
    anisotropy = material.cc_ccrgh_aniso.b;
}

bool is_transparent(in PbrMaterial material) {
    return map_bits(material) & MATERIAL_IS_TRANSPARENT_BIT;
}

#endif