#ifndef RNG_H_INCLUDED
#define RNG_H_INCLUDED

#include <cassert>
#include <cstdint>

// References: https://prng.di.unimi.it/
//             https://en.wikipedia.org/wiki/Xorshift

class Xshiro128
{
  public:
    inline Xshiro128();
    inline Xshiro128(uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3);

    inline uint32_t next();

  private:
    uint32_t s[4];
};

class Splitmix32
{

  public:
    inline Splitmix32();
    inline Splitmix32(uint32_t state);

    inline uint32_t next();

  private:
    uint32_t state;
};

#endif