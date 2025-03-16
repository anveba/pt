#ifndef PATHTRACE_HLSLI_INCLUDED
#define PATHTRACE_HLSLI_INCLUDED

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
    [[vk::location(0)]] float4 scatter; // w component is distance
    [[vk::location(1)]] float3 emission;
    [[vk::location(2)]] uint4 rng_state;
    [[vk::location(3)]] float3 next_direction;
};

uint taus_step(inout uint z, int s1, int s2, int s3, uint m)
{
    uint b = (((z << s1) ^ z) >> s2);
    z = ((z & m) << s3) ^ b;
    return z;
}

uint lcg_step(inout uint z, uint a, uint c)
{
    z = a * z + c;
    return z;
}

float hybrid_taus(inout uint4 state)
{
    return 2.3283064365387e-10 * (            
        taus_step(state.x, 13, 19, 12, 4294967294UL) ^  
        taus_step(state.y, 2, 25, 4, 4294967288UL) ^   
        taus_step(state.z, 3, 11, 17, 4294967280UL) ^  
        lcg_step(state.w, 1664525, 1013904223UL)      
    );
}

//TODO look at different RNGs
// uint rand_xorshift(inout uint rng_state)
// {
//     rng_state ^= (rng_state << 13);
//     rng_state ^= (rng_state >> 17);
//     rng_state ^= (rng_state << 5);
//     return rng_state;
// }

// float rand_float(inout uint4 rng_state) {
//     return (float)rand_xorshift(rng_state) / (float)0xFFFFFFFF;
// }

// float rand_float(inout uint rng_state) {
//     return (float)((1977654935 * rng_state) & 0x7FFFFFFF) / (float)0x80000000;
// }

float3 rand_dir(inout uint4 rng_state) {
    //TODO try Box-Muller transform
    float theta = hybrid_taus(rng_state) * PI;
    float phi = hybrid_taus(rng_state) * 2.0 * PI;
    return float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

uint4 get_seed(uint4 v0, uint4 v1) {
    const uint n = 16;
    uint s0 = 0;
    for  (uint i = 0; i < n; i++) { 
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4); 
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e); 
    } 
    return v0;
}

float3 cosine_weighted_rand_dir(inout uint4 rng_state, float3 n) {
    // TODO use more accurate method that uses tangent vectors
    float a = 1.0 - 2.0 * hybrid_taus(rng_state);
    float b = sqrt(1 - a * a);
    float phi = 2.0 * PI * hybrid_taus(rng_state);
    return float3(n.x + b * cos(phi), n.y + b * sin(phi), n.z + a);
}

#endif