#ifndef SCENE_LIGHTSAMPLE_H_INCLUDED
#define SCENE_LIGHTSAMPLE_H_INCLUDED

#include "aliastable.h"
#include "constants.h"
#include "scene.h"
#include "scenebuffer.h"

struct InfiniteLightData
{
    Vec3 position_direction;
    Vec3 emittance;
};

struct EmitterObjectData
{
    uint32_t table_offset;
    uint32_t table_bin_count;
    uint32_t instance_offset;
    uint32_t instance_count;
};

struct EmitterTriangleData
{
    uint32_t index_a;
    uint32_t index_b;
    uint32_t index_c;
};

struct LightBinData
{
    uint32_t light_type;
    union
    {
        InfiniteLightData infinite_light;
        EmitterObjectData emitter_object;
    };
};

class TableLightSampler
{
  public:
    TableLightSampler(const Scene& scene, const uint32_t* instance_offsets);

    size_t light_count() const { return table.size(); };

    static size_t size_in_bytes(const Scene& scene);
    size_t size_in_bytes() const;
    size_t copy(void* dest) const;

  private:
    AliasTable<LightBinData> table;
    std::vector<AliasTable<EmitterTriangleData>> emitter_tables;

    static constexpr uint32_t granularity = 4;
    static_assert(AliasTable<LightBinData>::bin_size() % granularity == 0);
    static_assert(AliasTable<EmitterTriangleData>::bin_size() % granularity == 0);
    static_assert(AliasTable<LightBinData>::bin_size() / granularity == LIGHT_BIN_SIZE);
    static_assert(AliasTable<EmitterTriangleData>::bin_size() / granularity == EMITTER_BIN_SIZE);
};

#endif