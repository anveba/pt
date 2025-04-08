#ifndef ALIASTABLE_H_INCLUDED
#define ALIASTABLE_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <vector>

template<typename T>
class AliasTable
{
  public:
    AliasTable() {}

    AliasTable(const T* values, const float* weights, size_t size)
    {
        build(values, weights, size);
    }

    void build(const T* values, const float* weights, size_t size)
    {
        assert(size > 0);

        bins.resize(size);
        double inv_sum = 1.0 / std::accumulate(weights, weights + size, 0.0);
        for (size_t i = 0; i < size; i++) {
            bins[i].p = weights[i] * inv_sum;
            bins[i].value = values[i];
        }

        // TODO
        for (size_t i = 0; i < size; i++) {
            bins[i].p = 1.0f / size;
            bins[i].q = 1.0f;
        }
    }

    inline size_t size() const { return bins.size(); }
    static constexpr size_t bin_size() { return sizeof(Bin); }
    static inline size_t size_in_bytes(size_t bin_count) { return bin_count * bin_size(); }
    inline size_t size_in_bytes() const { return size() * bin_size(); }

    inline size_t copy(void* dest) const
    {
        size_t sz = size_in_bytes();
        memcpy(dest, bins.data(), sz);
        return sz;
    }

  private:
    struct Bin
    {
        float p, q;
        uint32_t alias;
        T value;
    };

    std::vector<Bin> bins;
};

#endif