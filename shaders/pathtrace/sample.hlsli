#ifndef SAMPLE_HLSLI_INCLUDED
#define SAMPLE_HLSLI_INCLUDED

#include "rng.hlsli"
#include "util.hlsli"

float3 cosine_weighted_rand_dir(inout Rng rng) {
    float u1 = next_float(rng);
    float u2 = next_float(rng);
    float x = sqrt(u1) * cos(2.0 * PI * u2);
    float y = sqrt(u1) * sin(2.0 * PI * u2);
    float z = sqrt(1.0 - u1);
    return float3(x, y, z);
}

// https://hal.science/hal-01509746/document
// Assumes v.z is positive
float3 sample_visible_micronormal(float3 v, float2 alpha, inout Rng rng) {

    v = normalize(float3(alpha.x * v.x, alpha.y * v.y, v.z));

    float3 x = (v.z < 0.99999) ? normalize(cross(float3(0.0, 0.0, 1.0), v)) : float3(1.0, 0.0, 0.0);
    float3 y = cross(v, x);

    float u1 = next_float(rng);
    float u2 = next_float(rng);

    float a = 1.0 / (1.0 + v.z);
    float r = sqrt(u1);
    float phi = (u2 < a) ? u2 / a * PI : PI + (u2 - a) / (1.0 - a) * PI;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi) * ((u2 < a) ? 1.0 : v.z);

    float3 m = p1 * x + p2 * y + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * v;
    return normalize(float3(alpha.x * m.x, alpha.y * m.y, max(1e-6, m.z)));
}

float3 sample_micronormal(float2 alpha, inout Rng rng) {

    float phi = 2.0 * PI * next_float(rng);
    float u = min(next_float(rng), 0.99999);
    float l = sqrt(u / (1.0 - u));
    float x = alpha.x * cos(phi) * l;
    float y = alpha.y * sin(phi) * l;

    return normalize(float3(x, y, 1.0));
}

float power_heuristic(float p_i, float p_j) {
    return sq(p_i) / (sq(p_i) + sq(p_j));
}

float balance_heuristic(float p_i, float p_j) {
    return p_i / (p_i + p_j);
}

#endif