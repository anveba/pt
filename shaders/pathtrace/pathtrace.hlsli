#ifndef PATHTRACE_HLSLI_INCLUDED
#define PATHTRACE_HLSLI_INCLUDED

#include "rng.hlsli"

#define ORIGIN_OFFSET (1e-5)

struct UniformBufferObject
{
    float4 environment_colour;
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

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    // On input, the w-component is 1 on the final bounce, else 0. On output, it is the distance.
    [[vk::location(0)]] float4 throughput; 
    [[vk::location(1)]] float3 emission;
    [[vk::location(2)]] Rng rng;
    [[vk::location(3)]] float3 intersection;
    [[vk::location(4)]] float3 incoming_direction;
};

bool is_final_bounce(in RayPayload payload) {
    return payload.throughput.w != 0.0;
}

void set_final_bounce(inout RayPayload payload, bool b) {
    payload.throughput.w = b ? 1.0 : 0.0;
}

#endif