#ifndef RNG_HLSLI_INCLUDED
#define RNG_HLSLI_INCLUDED

struct Rng
{
    uint4 state;
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

// From GPU Gems 3, chapter 37
float hybrid_taus(inout uint4 state)
{
    return 2.3283064365387e-10 * (            
        taus_step(state.x, 13, 19, 12, 4294967294UL) ^  
        taus_step(state.y, 2, 25, 4, 4294967288UL) ^   
        taus_step(state.z, 3, 11, 17, 4294967280UL) ^  
        lcg_step(state.w, 1664525, 1013904223UL)      
    );
}

float xorshift(inout uint rng_state)
{
    rng_state ^= (rng_state << 13);
    rng_state ^= (rng_state >> 17);
    rng_state ^= (rng_state << 5);
    return (float)rng_state / (float)0xFFFFFFFF;
}

// Uses Tiny Encryption Algorithm.
Rng create_rng(uint4 v0, uint4 v1) {
    Rng rng;
    const uint n = 16;
    uint s0 = 0;
    for  (uint i = 0; i < n; i++) { 
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4); 
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e); 
    } 
    rng.state = v0;
    return rng;
}

uint rotl(uint x, int k)
{
    return (x << k) | (x >> (32 - k));
}

// References: https://prng.di.unimi.it/
//             https://en.wikipedia.org/wiki/Xorshift
uint xshiro128(inout uint4 s)
{
    const uint32_t result = rotl(s.x + s.w, 7) + s.x;

    const uint32_t t = s.y << 9;

    s.zwyx ^= s;

    s.z ^= t;

    s.w = rotl(s.w, 11);

    return result;
}

float next_float(inout Rng rng) {
    return hybrid_taus(rng.state);
}

uint splitmix(inout uint state)
{
    uint z = (state += 0x9e3779b9);
    z = (z ^ (z >> 16)) * 0x85ebca6b;
    z = (z ^ (z >> 13)) * 0xc2b2ae35;
    return z ^ (z >> 16);
}

#endif