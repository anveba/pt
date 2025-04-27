#ifndef INITABLEARRAY_H_INCLUDED
#define INITABLEARRAY_H_INCLUDED

#include "util.h"
#include <cassert>

template<typename T, size_t N>
class InitableArray
{
  public:
    template<class... Args>
    InitableArray(Args&&... args)
    {
        for (size_t i = 0; i < N; i++) {
            T& t = ((T*)sets)[i];
            new (&t) T(std::forward<Args>(args)...);
        }
    }

    ~InitableArray()
    {
        for (size_t i = 0; i < N; i++) {
            T& t = ((T*)sets)[i];
            t.~T();
        }
    }

    inline T& operator[](size_t i)
    {
        assert(i < N);
        return ((T*)sets)[i];
    }

    inline const T& operator[](size_t i) const
    {
        assert(i < N);
        return ((T*)sets)[i];
    }

    constexpr size_t size() const { return N; }

  private:
    uint8_t sets[N * sizeof(T)];

    NO_COPY(InitableArray);
};

#endif