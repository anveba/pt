#ifndef SCENE_SCENEBUFFER_H_INCLUDED
#define SCENE_SCENEBUFFER_H_INCLUDED

#include "graphics/cmdpool.h"
#include "scene.h"

struct BufferIndices
{
    uint32_t vertex;
    uint32_t index;
};

struct InstanceData
{
    alignas(4) uint32_t vertex_index;
    alignas(4) uint32_t index_index;
    alignas(4) uint32_t emitter_index;
    alignas(4) uint32_t material_index;
    alignas(16) Mat4 transform;
    alignas(16) Mat3x4 normal;

    static VkVertexInputBindingDescription binding_description(uint32_t binding);
    static std::array<VkVertexInputAttributeDescription, 7> attribute_descriptions(uint32_t binding, uint32_t location_offset);
};

class SceneBuffer
{
  public:
    SceneBuffer(Device& device,
                CommandPool& command_pool,
                const Scene& scene,
                VkBufferUsageFlagBits buffer_usage,
                VkMemoryAllocateFlags allocate_flags = 0);
    ~SceneBuffer();

    void rebuild(CommandPool& command_pool, const Scene& scene);

    inline const VkBuffer& handle() const { return buffer; }

    inline VkDeviceSize get_vertex_offset() const { return 0; }
    inline VkDeviceSize get_index_offset() const { return index_offset; }
    inline VkDeviceSize get_instance_offset() const { return instance_offset; }
    inline VkDeviceSize get_material_offset() const { return material_offset; }
    inline VkDeviceSize get_light_sampler_offset() const { return light_sampler_offset; }

    inline VkDeviceSize vertex_region_size() const { return index_offset; }
    inline VkDeviceSize index_region_size() const { return instance_offset - index_offset; }
    inline VkDeviceSize instance_region_size() const { return material_offset - instance_offset; }
    inline VkDeviceSize material_region_size() const { return light_sampler_offset - material_offset; }
    inline VkDeviceSize light_sampler_size() const { return buffer_size - light_sampler_offset; }
    inline VkDeviceSize get_buffer_size() const { return buffer_size; }

    // Goes from i = 0 to i = number of object variants, inclusive. The last element contains the end indices.
    inline const BufferIndices& get_start_indices(size_t i) const { return start_indices[i]; }
    inline size_t object_variant_count() const { return instance_offsets.size() - 1; }
    // i refers the the object variant index.
    inline size_t instance_count_of(size_t i) const { return instance_offsets[i + 1] - instance_offsets[i]; }
    inline u_int32_t get_light_count() const { return light_count; }

  private:
    Device& device;

    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
    VkBufferUsageFlagBits buffer_usage;
    VkMemoryAllocateFlags buffer_allocate_flags;

    VkDeviceSize index_offset;
    VkDeviceSize instance_offset;
    VkDeviceSize material_offset;
    VkDeviceSize light_sampler_offset;
    VkDeviceSize buffer_size;

    std::vector<BufferIndices> start_indices;
    std::vector<uint32_t> instance_offsets;
    uint32_t light_count;

    void build(CommandPool& command_pool, const Scene& scene);
    void destroy();

    NO_COPY(SceneBuffer);
};

#endif
