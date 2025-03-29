#ifndef PATHTRACE_HLSLI_INCLUDED
#define PATHTRACE_HLSLI_INCLUDED

#include "rng.hlsli"

#define PI (3.141592654)
#define INFINITY (1.0 / 0.0)

struct UniformBufferObject
{
    float4x4 inv_view;
    float4x4 inv_proj;
    float near;
    float far;
    float old_samples_mult;
    float new_samples_mult;
    uint4 seed;
    uint samples;
    uint max_bounces;
};

struct PbrMaterial {
    float4 base_colour;
    float4 emission;
    float4 specular;
    float4 sheen;
    float4 clearcoat;
    float4 metalness_anisotropy;
    uint4 col_emi_rgh_spec_maps;
    uint4 shn_clcoat_metal_norm_maps;
};

struct InstanceData {
    uint vertex_index;
    uint index_index;
    uint material_index;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    [[vk::location(0)]] float4 brdf; // w component is pdf. The cosine term is baked in.
    [[vk::location(1)]] float3 emission;
    [[vk::location(2)]] Rng rng;
    [[vk::location(3)]] float4 incoming_direction; // w component is distance
};

#endif