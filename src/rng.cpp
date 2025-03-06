#include "rng.h"

#include <cassert>
#include <cstdint>

// References: https://prng.di.unimi.it/
//             https://en.wikipedia.org/wiki/Xorshift

constexpr uint32_t rotl(const uint32_t x, int k)
{
    return (x << k) | (x >> (32 - k));
}

inline Xshiro128::Xshiro128() {}

inline Xshiro128::Xshiro128(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;
}

inline uint32_t Xshiro128::next()
{
    const uint32_t result = rotl(s[0] + s[3], 7) + s[0];

    const uint32_t t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = rotl(s[3], 11);

    return result;
}

inline Splitmix32::Splitmix32() {}

inline Splitmix32::Splitmix32(uint32_t state)
    : state(state)
{
}

inline uint32_t Splitmix32::next()
{
    uint32_t z = (state += 0x9e3779b9);
    z = (z ^ (z >> 16)) * 0x85ebca6b;
    z = (z ^ (z >> 13)) * 0xc2b2ae35;
    return z ^ (z >> 16);
}