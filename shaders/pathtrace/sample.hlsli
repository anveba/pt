#ifndef SAMPLE_HLSLI_INCLUDED
#define SAMPLE_HLSLI_INCLUDED

#include "rng.hlsli"

float3 cosine_weighted_rand_dir(inout Rng rng) {
    float u1 = next_float(rng);
    float u2 = next_float(rng);
    float x = sqrt(u1) * cos(2.0 * PI * u2);
    float y = sqrt(u1) * sin(2.0 * PI * u2);
    float z = sqrt(1.0 - u1);
    return float3(x, y, z);
}

#endif