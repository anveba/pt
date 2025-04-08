#include "scenebuffer.h"
#include "lightsample.h"

VkVertexInputBindingDescription InstanceData::binding_description(uint32_t binding)
{
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = binding;
    binding_description.stride = sizeof(InstanceData);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return binding_description;
}

std::array<VkVertexInputAttributeDescription, 7> InstanceData::attribute_descriptions(uint32_t binding, uint32_t location_offset)
{
    std::array<VkVertexInputAttributeDescription, 7> attribute_descriptions;
    attribute_descriptions[0].binding = binding;
    attribute_descriptions[0].location = location_offset + 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(InstanceData, transform) + sizeof(float) * 4 * 0;
    attribute_descriptions[1].binding = binding;
    attribute_descriptions[1].location = location_offset + 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(InstanceData, transform) + sizeof(float) * 4 * 1;
    attribute_descriptions[2].binding = binding;
    attribute_descriptions[2].location = location_offset + 2;
    attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[2].offset = offsetof(InstanceData, transform) + sizeof(float) * 4 * 2;
    attribute_descriptions[3].binding = binding;
    attribute_descriptions[3].location = location_offset + 3;
    attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attribute_descriptions[3].offset = offsetof(InstanceData, transform) + sizeof(float) * 4 * 3;
    attribute_descriptions[4].binding = binding;
    attribute_descriptions[4].location = location_offset + 4;
    attribute_descriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[4].offset = offsetof(InstanceData, normal) + sizeof(float) * 4 * 0;
    attribute_descriptions[5].binding = binding;
    attribute_descriptions[5].location = location_offset + 5;
    attribute_descriptions[5].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[5].offset = offsetof(InstanceData, normal) + sizeof(float) * 4 * 1;
    attribute_descriptions[6].binding = binding;
    attribute_descriptions[6].location = location_offset + 6;
    attribute_descriptions[6].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[6].offset = offsetof(InstanceData, normal) + sizeof(float) * 4 * 2;
    return attribute_descriptions;
}

SceneBuffer::SceneBuffer(
    Device& device,
    CommandPool& command_pool,
    const Scene& scene,
    VkBufferUsageFlagBits buffer_usage,
    VkMemoryAllocateFlags buffer_allocate_flags)
    : device(device)
    , buffer_usage(buffer_usage)
    , buffer_allocate_flags(buffer_allocate_flags)
{
    build(command_pool, scene);
}

SceneBuffer::~SceneBuffer()
{
    destroy();
}

void SceneBuffer::build(CommandPool& command_pool, const Scene& scene)
{
    size_t vertex_count = 0, tri_count = 0, instance_count = 0, material_count = scene.get_materials().size();
    const std::vector<ObjectVariant>& object_variants = scene.get_object_variants();

    for (const Mesh& mesh : scene.get_meshes()) {
        vertex_count += mesh.get_vertices().size();
        tri_count += mesh.get_indexed_triangles().size();
    }

    for (const ObjectVariant& variant : object_variants)
        instance_count += variant.instances.size();

    const VkDeviceSize alignment = device.get_physical_device_info().properties.limits.minStorageBufferOffsetAlignment;

    const size_t vertex_region_size = round_up_to<size_t>(vertex_count * sizeof(Vertex), alignment);
    const size_t index_region_size = round_up_to<size_t>(tri_count * sizeof(IndexedTriangle), alignment);
    const size_t instance_region_size = round_up_to<size_t>(instance_count * sizeof(InstanceData), alignment);
    const size_t material_region_size = round_up_to<size_t>(material_count * sizeof(PbrMaterial), alignment);
    const size_t light_sampler_size = round_up_to<size_t>(TableLightSampler::size_in_bytes(scene), alignment);

    const size_t total_buffer_size = vertex_region_size +
                                     index_region_size +
                                     instance_region_size +
                                     material_region_size +
                                     light_sampler_size;
    uint8_t* const all_data = new uint8_t[total_buffer_size];

    Vertex* const vertex_data = (Vertex*)all_data;
    uint32_t* const index_data = (uint32_t*)((uint8_t*)vertex_data + vertex_region_size);
    InstanceData* const instance_data = (InstanceData*)((uint8_t*)index_data + index_region_size);

    start_indices.resize(scene.get_meshes().size() + 1);
    start_indices[0] = { .vertex = 0, .index = 0 };

    for (size_t i = 0; i < scene.get_meshes().size(); i++) {

        const Mesh& mesh = scene.get_meshes()[i];

        size_t vertex_start = start_indices[i].vertex;
        size_t index_start = start_indices[i].index;

        memcpy(vertex_data + vertex_start, mesh.get_vertices().data(), mesh.get_vertices().size() * sizeof(Vertex));
        memcpy(index_data + index_start, mesh.get_indexed_triangles().data(), mesh.get_indexed_triangles().size() * sizeof(IndexedTriangle));

        start_indices[i + 1].vertex = vertex_start + mesh.get_vertices().size();
        start_indices[i + 1].index = index_start + mesh.get_indexed_triangles().size() * 3;
    }

    assert(start_indices.back().vertex == vertex_count);
    assert(start_indices.back().index == tri_count * 3);

    instance_offsets.resize(object_variants.size() + 1);
    instance_offsets[0] = 0;
    uint32_t emitter_index = 0;

    for (size_t i = 0; i < object_variants.size(); i++) {

        const ObjectVariant& variant = object_variants[i];

        for (size_t j = 0; j < variant.instances.size(); j++) {
            InstanceData& d = instance_data[instance_offsets[i] + j];
            d.vertex_index = start_indices[i].vertex;
            d.index_index = start_indices[i].index;
            d.emitter_index = scene.get_materials()[variant.material_index].is_emitter() ? emitter_index++ : NO_EMITTER_INDEX;
            d.material_index = variant.material_index;
            d.transform = glm::transpose(variant.instances[j].transform.matrix);
            d.normal = glm::transpose(glm::inverse(d.transform));
        }

        instance_offsets[i + 1] = instance_offsets[i] + variant.instances.size();
    }

    PbrMaterial* const material_data = (PbrMaterial*)((uint8_t*)instance_data + instance_region_size);
    memcpy(material_data, scene.get_materials().data(), scene.get_materials().size() * sizeof(PbrMaterial));

    if (light_sampler_size > 0) {
        uint8_t* const light_sampler_data = ((uint8_t*)material_data + material_region_size);
        TableLightSampler light_sampler(scene, instance_offsets.data());
        light_sampler.copy(light_sampler_data);
        light_count = light_sampler.light_count();
    } else {
        light_count = 0;
    }

    device.create_buffer(buffer,
                         buffer_memory,
                         total_buffer_size,
                         buffer_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         buffer_allocate_flags);

    command_pool.transfer_to_buffer(buffer,
                                    all_data,
                                    total_buffer_size);

    index_offset = vertex_region_size;
    instance_offset = index_offset + index_region_size;
    material_offset = instance_offset + instance_region_size;
    light_sampler_offset = material_offset + material_region_size;
    buffer_size = total_buffer_size;

    delete[] all_data;
}

void SceneBuffer::destroy()
{
    vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
    vkFreeMemory(device.logical_handle(), buffer_memory, nullptr);
}

void SceneBuffer::rebuild(CommandPool& command_pool, const Scene& scene)
{
    destroy();
    build(command_pool, scene);
}