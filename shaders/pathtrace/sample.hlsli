#ifndef SAMPLE_HLSLI_INCLUDED
#define SAMPLE_HLSLI_INCLUDED

#include "rng.hlsli"
#include "util.hlsli"

#define USE_SOBOL
#define SOBOL_MAX_DIMENSION (4)

// Implementation based on Practical Hash-based Owen Scrambling (Burley 2020).

#ifdef USE_SOBOL
#define SAMPLER SobolSampler
#else
#define SAMPLER SimpleSampler
#endif

struct SimpleSampler {
    Rng rng;
};

struct SobolSampler {
    uint index;
    uint dimension;
    uint shuffled_index;
    uint seed;
};

static const uint directions[SOBOL_MAX_DIMENSION][32] = {
    0x80000000, 0x40000000, 0x20000000, 0x10000000,
    0x08000000, 0x04000000, 0x02000000, 0x01000000,
    0x00800000, 0x00400000, 0x00200000, 0x00100000,
    0x00080000, 0x00040000, 0x00020000, 0x00010000,
    0x00008000, 0x00004000, 0x00002000, 0x00001000,
    0x00000800, 0x00000400, 0x00000200, 0x00000100,
    0x00000080, 0x00000040, 0x00000020, 0x00000010,
    0x00000008, 0x00000004, 0x00000002, 0x00000001,

    0x80000000, 0xc0000000, 0xa0000000, 0xf0000000,
    0x88000000, 0xcc000000, 0xaa000000, 0xff000000,
    0x80800000, 0xc0c00000, 0xa0a00000, 0xf0f00000,
    0x88880000, 0xcccc0000, 0xaaaa0000, 0xffff0000,
    0x80008000, 0xc000c000, 0xa000a000, 0xf000f000,
    0x88008800, 0xcc00cc00, 0xaa00aa00, 0xff00ff00,
    0x80808080, 0xc0c0c0c0, 0xa0a0a0a0, 0xf0f0f0f0,
    0x88888888, 0xcccccccc, 0xaaaaaaaa, 0xffffffff,

    0x80000000, 0xc0000000, 0x60000000, 0x90000000,
    0xe8000000, 0x5c000000, 0x8e000000, 0xc5000000,
    0x68800000, 0x9cc00000, 0xee600000, 0x55900000,
    0x80680000, 0xc09c0000, 0x60ee0000, 0x90550000,
    0xe8808000, 0x5cc0c000, 0x8e606000, 0xc5909000,
    0x6868e800, 0x9c9c5c00, 0xeeee8e00, 0x5555c500,
    0x8000e880, 0xc0005cc0, 0x60008e60, 0x9000c590,
    0xe8006868, 0x5c009c9c, 0x8e00eeee, 0xc5005555,

    0x80000000, 0xc0000000, 0x20000000, 0x50000000,
    0xf8000000, 0x74000000, 0xa2000000, 0x93000000,
    0xd8800000, 0x25400000, 0x59e00000, 0xe6d00000,
    0x78080000, 0xb40c0000, 0x82020000, 0xc3050000,
    0x208f8000, 0x51474000, 0xfbea2000, 0x75d93000,
    0xa0858800, 0x914e5400, 0xdbe79e00, 0x25db6d00,
    0x58800080, 0xe54000c0, 0x79e00020, 0xb6d00050,
    0x800800f8, 0xc00c0074, 0x200200a2, 0x50050093,
};

float simple_sample_1d(inout SimpleSampler s) {
    return next_float(s.rng);
}

float2 simple_sample_2d(inout SimpleSampler s) {
    return float2(next_float(s.rng), next_float(s.rng));
}

void simple_sampler_start_new(inout SimpleSampler s, uint2 pixel, uint sample_index) {
    uint4 s0 = uint4(0x2ce9f109, 0x14757dc9, 0x40b8b0ab, 0x422c5873);
    uint4 s1 = uint4(0xd8a0e72c, 0xb1de11fd, 0x9f8df97f, 0x7dd7456e);

    s0 = hash_combine(s0, pixel.x);
    s0 = hash_combine(s0, pixel.y);
    s0 = hash_combine(s0, sample_index);

    s1 = hash_combine(s1, pixel.x);
    s1 = hash_combine(s1, pixel.y);
    s1 = hash_combine(s1, sample_index);

    s.rng = create_rng(s0, s1);
}

uint laine_karras_permutation(uint x, uint seed) {
    x += seed;
    x ^= x * 0x6c50b47cu;
    x ^= x * 0xb82f1e52u;
    x ^= x * 0xc7afe638u;
    x ^= x * 0x8d22f6e6u;
    return x;
}

uint scramble(uint x, uint seed) {
    x = reversebits(x);
    x = laine_karras_permutation(x, seed);
    x = reversebits(x);
    return x;
}

uint sobol(uint index, uint dim)
{
    uint x = 0;
    for (int b = 0; b < 32; b++) {
      uint mask = (index >> b) & 1;
      if (mask)
        x ^= directions[dim][b];
    }
    return x;
}

uint scrambled_sobol(uint shuffled, uint dimension, uint seed) {
    uint x = sobol(shuffled, dimension);
    return scramble(x, hash_combine(seed, dimension));
}

void pad(inout SobolSampler s) {
    s.dimension = 0;
    s.seed = hash(s.seed);
    s.shuffled_index = scramble(s.index, s.seed);
}

float sobol_sample_1d(inout SobolSampler s) {
    if (s.dimension >= SOBOL_MAX_DIMENSION) 
        pad(s);
    float result = 0x1p-32 * scrambled_sobol(s.shuffled_index, s.dimension, s.seed);
    s.dimension += 1;
    return min(result, JUST_BELOW_ONE);
}

float2 sobol_sample_2d(inout SobolSampler s) {
    if (s.dimension + 1 >= SOBOL_MAX_DIMENSION) 
        pad(s);
    float2 result = 0x1p-32 * float2(
        scrambled_sobol(s.shuffled_index, s.dimension + 0, s.seed), 
        scrambled_sobol(s.shuffled_index, s.dimension + 1, s.seed));
    s.dimension += 2;
    return min(result, JUST_BELOW_ONE);
}

void sobol_sampler_start_new(inout SobolSampler s, uint2 pixel, uint sample_index) {
    s.seed = hash_combine(pixel.x, pixel.y);
    s.dimension = 0x00FFFFFF;
    s.index = sample_index;
}

float sample_1d(inout SAMPLER s) {
    #ifdef USE_SOBOL
    return sobol_sample_1d(s);
    #else
    return simple_sample_1d(s);
    #endif
}

float2 sample_2d(inout SAMPLER s) {
    #ifdef USE_SOBOL
    return sobol_sample_2d(s);
    #else
    return simple_sample_2d(s);
    #endif
}

void sampler_start_new(inout SAMPLER s, uint2 pixel, uint sample_index) {
    #ifdef USE_SOBOL
    sobol_sampler_start_new(s, pixel, sample_index);
    #else
    simple_sampler_start_new(s, pixel, sample_index);
    #endif
}

// From Ray Tracing Gems, 2019, Chapter 16
float3 cosine_weighted_rand_dir(float2 u) {
    float x = sqrt(u.x) * cos(2.0 * PI * u.y);
    float y = sqrt(u.x) * sin(2.0 * PI * u.y);
    float z = sqrt(1.0 - u.x);
    return float3(x, y, z);
}

// From Ray Tracing Gems, 2019, Chapter 16
float3 random_barycentric(float2 u) {
    float beta = 1.0 - sqrt(u.x); 
    float gamma = (1.0 - beta) * u.y;
    float alpha = 1.0 - beta - gamma;
    return float3(alpha, beta, gamma);
}

// From Ray Tracing Gems, 2019, Chapter 16
float2 sample_concentric_disk(float2 u) {
    float a = 2.0 * u.x - 1.0;
    float b = 2.0 * u.y - 1.0;
    float r, phi;
    if (a*a > b*b) {
        r = a;
        phi = (PI / 4.0) * (b / a);
    } else {
        r = b;
        phi = (PI / 2.0) - (PI / 4.0) * (a / b);
    }
    return r * float2(cos(phi), sin(phi));
}

// From Ray Tracing Gems, 2019, Chapter 16
float3 sample_triangle(float3 p_a, float3 p_b, float3 p_c, float2 u) {
    float3 bary = random_barycentric(u);
    return bary.x * p_a + bary.y * p_b + bary.z * p_c;
}

// https://hal.science/hal-01509746/document
// Assumes v.z is positive
float3 sample_visible_micronormal(float3 v, float2 alpha, float2 u) {

    v = normalize(float3(alpha.x * v.x, alpha.y * v.y, v.z));

    float3 x = (v.z < 0.99999) ? normalize(cross(float3(0.0, 0.0, 1.0), v)) : float3(1.0, 0.0, 0.0);
    float3 y = cross(v, x);

    float a = 1.0 / (1.0 + v.z);
    float r = sqrt(u.x);
    float phi = (u.y < a) ? u.y / a * PI : PI + (u.y - a) / (1.0 - a) * PI;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi) * ((u.y < a) ? 1.0 : v.z);

    float3 m = p1 * x + p2 * y + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * v;
    return normalize(float3(alpha.x * m.x, alpha.y * m.y, max(1e-6, m.z)));
}

float3 sample_micronormal(float2 alpha, float2 u) {

    float phi = 2.0 * PI * u.x;
    float l = sqrt(u.y / (1.0 - u.y));
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