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
    uint light_count;
};

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    // The w-component is 1 if the path terminates, else 0.
    [[vk::location(0)]] float4 throughput; 
    [[vk::location(1)]] float3 radiance;
    [[vk::location(2)]] Rng rng;
    [[vk::location(3)]] float3 intersection;
    [[vk::location(4)]] float3 incoming_direction;
    // The first component is the PDF of the BRDF. It will be -1 for the first ray. 
    // The second component is the PDF of the distribution that the direction was chosen from.
    [[vk::location(5)]] float2 pdf; 
};

struct ShadowRayPayload
{
    [[vk::location(0)]] bool is_occluded; 
};

RayPayload create_ray_payload(in Rng rng) {
    RayPayload payload;
    payload.radiance.rgb = 0.0;
    payload.rng = rng;
    return payload;
}

void set_new_path(inout RayPayload payload) {
    payload.throughput.rgb = 1.0;
    payload.pdf.x = -1.0;
}

bool is_first_ray(in RayPayload payload) {
    return payload.pdf.x < 0.0;
}

bool is_final_segment(in RayPayload payload) {
    return payload.throughput.w != 0.0;
}

void set_new_segment(inout RayPayload payload, bool is_final) {
    payload.throughput.w = is_final ? 1.0 : 0.0;
}

void accumulate_throughput(inout RayPayload payload, float3 throughput) {
    payload.throughput.rgb *= throughput;
}

void add_radiance(inout RayPayload payload, float3 radiance) {
    payload.radiance.rgb += payload.throughput.rgb * radiance;
}

void no_scatter(inout RayPayload payload) {
    payload.throughput.w = 1.0;
}

void set_next_ray(inout RayPayload payload, float3 intersection, float3 incoming_direction) {
    payload.intersection = intersection;
    payload.incoming_direction = incoming_direction;
}

void get_next_ray(in RayPayload payload, out float3 origin, out float3 direction) {
    origin = payload.intersection;
    direction = payload.incoming_direction;
}

float3 get_radiance(in RayPayload payload) {
    return payload.radiance.rgb;
}

float get_brdf_pdf(in RayPayload payload) {
    return payload.pdf.x;
}

void set_brdf_pdf(inout RayPayload payload, float pdf) {
    payload.pdf.x = pdf;
}

float3 get_throughput(in RayPayload payload) {
    return payload.throughput.rgb;
}

#endif