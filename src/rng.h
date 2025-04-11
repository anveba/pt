#ifndef RNG_H_INCLUDED
#define RNG_H_INCLUDED

#include <cassert>
#include <cstdint>

// References: https://prng.di.unimi.it/
//             https://en.wikipedia.org/wiki/Xorshift

class Xshiro128
{
  public:
    Xshiro128();
    Xshiro128(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);

    uint32_t next();
    float next_float();

  private:
    uint32_t s[4];
};

class Splitmix32
{

  public:
    Splitmix32();
    Splitmix32(uint32_t state);

    uint32_t next();

  private:
    uint32_t state;
};

#endif