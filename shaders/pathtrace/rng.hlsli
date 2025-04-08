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

float next_float(inout Rng rng) {
    return hybrid_taus(rng.state);
}

#endif