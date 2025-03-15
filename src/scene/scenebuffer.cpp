#include "scenebuffer.h"

VkVertexInputBindingDescription RasteriseInstanceData::binding_description(uint32_t binding)
{
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = binding;
    binding_description.stride = sizeof(RasteriseInstanceData);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return binding_description;
}

std::array<VkVertexInputAttributeDescription, 7> RasteriseInstanceData::attribute_descriptions(uint32_t binding, uint32_t location_offset)
{
    std::array<VkVertexInputAttributeDescription, 7> attribute_descriptions;
    attribute_descriptions[0].binding = binding;
    attribute_descriptions[0].location = location_offset + 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[0].offset = sizeof(float) * 4 * 0;
    attribute_descriptions[1].binding = binding;
    attribute_descriptions[1].location = location_offset + 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[1].offset = sizeof(float) * 4 * 1;
    attribute_descriptions[2].binding = binding;
    attribute_descriptions[2].location = location_offset + 2;
    attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[2].offset = sizeof(float) * 4 * 2;
    attribute_descriptions[3].binding = binding;
    attribute_descriptions[3].location = location_offset + 3;
    attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[3].offset = sizeof(float) * 4 * 3;
    attribute_descriptions[4].binding = binding;
    attribute_descriptions[4].location = location_offset + 4;
    attribute_descriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[4].offset = sizeof(float) * 4 * 4;
    attribute_descriptions[5].binding = binding;
    attribute_descriptions[5].location = location_offset + 5;
    attribute_descriptions[5].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[5].offset = sizeof(float) * 4 * 5;
    attribute_descriptions[6].binding = binding;
    attribute_descriptions[6].location = location_offset + 6;
    attribute_descriptions[6].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[6].offset = sizeof(float) * 4 * 6;
    return attribute_descriptions;
}

template<typename T>
inline void populate_instance_data(T& instance_data,
                                   const ObjectVariant& variant,
                                   size_t idx,
                                   uint32_t vertex_index,
                                   uint32_t index_index);

template<>
inline void populate_instance_data<PathTraceInstanceData>(
    PathTraceInstanceData& instance_data,
    const ObjectVariant& variant,
    size_t idx,
    uint32_t vertex_index,
    uint32_t index_index)
{
    instance_data.vertex_index = vertex_index;
    instance_data.index_index = index_index;
    instance_data.material_index = 0; // TODO
}

template<>
inline void populate_instance_data<RasteriseInstanceData>(
    RasteriseInstanceData& instance_data,
    const ObjectVariant& variant,
    size_t idx,
    uint32_t vertex_index,
    uint32_t index_index)
{
    instance_data.transform = variant.instances[idx].transform.matrix;
    instance_data.normal = glm::transpose(glm::inverse(instance_data.transform));
}

template<typename T>
inline SceneBuffer<T>::SceneBuffer(
    Device& device,
    CommandPool& command_pool,
    const Scene& scene,
    VkBufferUsageFlagBits buffer_usage,
    VkMemoryAllocateFlags allocate_flags)
    : device(device)
    , scene(&scene)
    , buffer_usage(buffer_usage)
    , allocate_flags(allocate_flags)
{
    build_buffers(command_pool);
}

template<typename T>
inline SceneBuffer<T>::~SceneBuffer()
{
    free();
}

template<typename T>
inline void SceneBuffer<T>::build_buffers(CommandPool& command_pool)
{
    assert(scene != nullptr);

    size_t vertex_count = 0, tri_count = 0, instance_count = 0;
    const std::vector<ObjectVariant>& object_variants = scene->get_object_variants();

    for (const ObjectVariant& variant : object_variants) {
        vertex_count += variant.mesh.get_vertices().size();
        tri_count += variant.mesh.get_indexed_triangles().size();
        instance_count += variant.instances.size();
    }

    const VkDeviceSize alignment = device.get_physical_device_info().properties.limits.minStorageBufferOffsetAlignment;
    const size_t vertex_region_size = round_up_to<size_t>(vertex_count * sizeof(Vertex), alignment);
    const size_t index_region_size = round_up_to<size_t>(tri_count * sizeof(IndexedTriangle), alignment);
    const size_t instance_region_size = instance_count * sizeof(T);

    const size_t total_buffer_size = vertex_region_size + index_region_size + instance_region_size;

    uint8_t* const all_data = new uint8_t[total_buffer_size];
    Vertex* const vertex_data = (Vertex*)all_data;
    uint32_t* const index_data = (uint32_t*)((uint8_t*)vertex_data + vertex_region_size);
    T* const instance_data = (T*)((uint8_t*)index_data + index_region_size);

    start_indices.resize(object_variants.size() + 1);

    uint32_t vertex_count_acc = 0, index_count_acc = 0, instance_count_acc = 0;
    start_indices[0] = { .vertex = vertex_count_acc, .index = index_count_acc, .instance = instance_count_acc };

    for (size_t i = 0; i < object_variants.size(); i++) {

        const ObjectVariant& variant = object_variants[i];

        memcpy(vertex_data + vertex_count_acc, variant.mesh.get_vertices().data(), variant.mesh.get_vertices().size() * sizeof(Vertex));
        memcpy(index_data + index_count_acc, variant.mesh.get_indexed_triangles().data(), variant.mesh.get_indexed_triangles().size() * sizeof(IndexedTriangle));

        for (size_t j = 0; j < variant.instances.size(); j++) {
            populate_instance_data(instance_data[instance_count_acc], variant, j, vertex_count_acc, index_count_acc);
            instance_count_acc++;
        }

        vertex_count_acc += variant.mesh.get_vertices().size();
        index_count_acc += variant.mesh.get_indexed_triangles().size() * 3;

        start_indices[i + 1] = { .vertex = vertex_count_acc, .index = index_count_acc, .instance = instance_count_acc };
    }

    assert(start_indices.back().vertex == vertex_count);
    assert(start_indices.back().index == tri_count * 3);
    assert(start_indices.back().instance == instance_count);

    device.create_buffer(buffer,
                         buffer_memory,
                         total_buffer_size,
                         buffer_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         allocate_flags);

    command_pool.transfer_to_buffer(buffer,
                                    all_data,
                                    total_buffer_size);

    index_offset = vertex_region_size;
    instance_offset = index_offset + index_region_size;
    buffer_size = total_buffer_size;

    delete[] all_data;
}

template<typename T>
inline void SceneBuffer<T>::free()
{
    vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
    vkFreeMemory(device.logical_handle(), buffer_memory, nullptr);
}

template<typename T>
inline void SceneBuffer<T>::rebuild(CommandPool& command_pool, const Scene& scene)
{
    free();
    this->scene = &scene;
    build_buffers(command_pool);
}

template class SceneBuffer<PathTraceInstanceData>;
template class SceneBuffer<RasteriseInstanceData>;