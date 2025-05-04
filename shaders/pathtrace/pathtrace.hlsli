#ifndef PATHTRACE_HLSLI_INCLUDED
#define PATHTRACE_HLSLI_INCLUDED

#include "sample.hlsli"

#define ORIGIN_OFFSET (1e-5)

struct UniformBufferObject
{
    float4x4 inv_view;
    float4x4 inv_proj;
    float near;
    float far;
    float focus_dist;
    float lens_radius;
    float exposure;

    uint sample_index;
    uint samples;
    uint max_bounces;

    uint width;
    uint height;
    uint queue_capacity;

    uint light_count;
    float4 environment_colour;

};

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    // The w-component is 1 if the path terminates, else 0.
    [[vk::location(0)]] float4 throughput;
    // The w-component is the PDF of the BRDF. It will be -1 for the first ray. 
    [[vk::location(1)]] float4 radiance;
    [[vk::location(2)]] SAMPLER sampler;
    [[vk::location(3)]] float3 intersection;
    [[vk::location(4)]] float3 incoming_direction;
};

struct ShadowRayPayload
{
    [[vk::location(0)]] bool is_occluded; 
};

inline RayPayload create_ray_payload() {
    RayPayload payload;
    payload.radiance.rgb = 0.0;
    return payload;
}

inline void set_new_path(inout RayPayload payload) {
    payload.throughput.rgb = 1.0;
    payload.radiance.w = -1.0;
}

inline bool is_first_ray(in RayPayload payload) {
    return payload.radiance.w < 0.0;
}

inline bool is_final_segment(in RayPayload payload) {
    return payload.throughput.w != 0.0;
}

inline void set_new_segment(inout RayPayload payload, bool is_final) {
    payload.throughput.w = is_final ? 1.0 : 0.0;
}

inline void accumulate_throughput(inout RayPayload payload, float3 throughput) {
    payload.throughput.rgb *= throughput;
}

inline void add_radiance(inout RayPayload payload, float3 radiance) {
    payload.radiance.rgb += payload.throughput.rgb * radiance.rgb;
}

inline void no_scatter(inout RayPayload payload) {
    payload.throughput.w = 1.0;
}

inline void set_next_ray(inout RayPayload payload, float3 intersection, float3 incoming_direction) {
    payload.intersection = intersection;
    payload.incoming_direction = incoming_direction;
}

inline void get_next_ray(in RayPayload payload, out float3 origin, out float3 direction) {
    origin = payload.intersection;
    direction = payload.incoming_direction;
}

inline float3 get_radiance(in RayPayload payload) {
    return payload.radiance.rgb;
}

inline float get_brdf_pdf(in RayPayload payload) {
    return payload.radiance.w;
}

inline void set_brdf_pdf(inout RayPayload payload, float pdf) {
    payload.radiance.w = pdf;
}

inline float3 get_throughput(in RayPayload payload) {
    return payload.throughput.rgb;
}

inline void set_throughput(inout RayPayload payload, float3 throughput) {
    payload.throughput.rgb = throughput;
}

bool russian_roulette(inout RayPayload payload) {
    const float3 throughput = get_throughput(payload);
    const float max_throughput = max(max(throughput.r, throughput.g), throughput.b);
    float u = sample_1d(payload.sampler);
    if (max_throughput < 1.0) {
        float q = max(0.0, 1.0 - max_throughput);
        if (u < q)
            return true;
        accumulate_throughput(payload, 1.0 / (1.0 - q));
    }
    return false;
}

float3 correct_radiance(float3 radiance) {
    if (radiance.r < 0.0 || !isfinite(radiance.r))
        radiance.r = 0.0;
    if (radiance.g < 0.0 || !isfinite(radiance.g))
        radiance.g = 0.0;
    if (radiance.b < 0.0 || !isfinite(radiance.b))
        radiance.b = 0.0;
    return radiance;
}

#endif