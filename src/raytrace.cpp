#include "raytrace.h"

#include "dispatch.h"
#include "scene.h"

#include <cassert>

struct ObjectData
{
    uint32_t vertex_index;
    uint32_t index_index;
    uint32_t material_index;
};

std::vector<VkDescriptorPoolSize> RayTracer::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
             { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 },
             { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
}

RayTracer::RayTracer(
    Device& device,
    Dispatcher& dispatcher,
    const Scene& scene,
    Shader& ray_gen,
    Shader& ray_miss,
    Shader& ray_closest_hit,
    VkExtent2D extent)
    : device(device)
    , dispatcher(dispatcher)
    , extent(extent)
    , scene(nullptr)
    , in_render(false)
{
    Splitmix32 sm(1);
    rng = Xshiro128(sm.next(), sm.next(), sm.next(), sm.next());

    create_dest_image(dispatcher);
    create_descriptor_set_layout();
    create_pipeline(ray_gen, ray_miss, ray_closest_hit);
    create_shader_binding_tables();

    device.create_buffer(uniform_buffer, uniform_buffer_memory, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(device.logical, uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, (void**)&uniform_map);

    set_scene(dispatcher, scene);
    create_descriptor_sets(dispatcher);
    create_command_buffer(dispatcher);
    create_sync_objects();

    set_samples(1);
    set_max_bounces(1);
}

RayTracer::~RayTracer()
{
    assert(scene != nullptr);

    free_scene_buffers();

    vkDestroyBuffer(device.logical, uniform_buffer, nullptr);
    vkUnmapMemory(device.logical, uniform_buffer_memory);
    vkFreeMemory(device.logical, uniform_buffer_memory, nullptr);

    destroy_dest_image();

    vkDestroySemaphore(device.logical, render_semaphore, nullptr);
    vkDestroyFence(device.logical, render_fence, nullptr);

    vkDestroyPipeline(device.logical, pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device.logical, descriptor_set_layout, nullptr);

    vkDestroyBuffer(device.logical, shader_group_buffer, nullptr);
    vkFreeMemory(device.logical, shader_group_buffer_memory, nullptr);
}

void RayTracer::create_dest_image(Dispatcher& dispatcher)
{
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM; // TODO: hdr
    device.create_image(dest_image,
                        dest_image_memory,
                        extent.width,
                        extent.height,
                        format,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    dest_image_view = device.create_image_view(dest_image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer command_buffer = dispatcher.begin_one_time_use_command_buffer();

    transition_image_layout(command_buffer,
                            dest_image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL,
                            0,
                            0,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    dispatcher.end_one_time_use_command_buffer(command_buffer, device.graphics_queue);
}

void RayTracer::destroy_dest_image()
{
    vkDestroyImageView(device.logical, dest_image_view, nullptr);
    vkDestroyImage(device.logical, dest_image, nullptr);
    vkFreeMemory(device.logical, dest_image_memory, nullptr);
}

void RayTracer::create_shader_binding_tables()
{
    const uint32_t aligned_handle_size = round_up_to<uint32_t>(
        device.physical_device_info.ray_tracing_properties.shaderGroupHandleSize,
        device.physical_device_info.ray_tracing_properties.shaderGroupHandleAlignment);
    const uint32_t group_count = static_cast<uint32_t>(shader_groups.size());
    const uint32_t table_size = group_count * aligned_handle_size;

    device.create_buffer(
        shader_group_buffer,
        shader_group_buffer_memory,
        table_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    std::vector<uint8_t> shader_handle_data;
    shader_handle_data.resize(table_size);
    if (vkGetRayTracingShaderGroupHandlesKHR(device.logical, pipeline, 0, group_count, table_size, shader_handle_data.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to get ray tracing shader group handles.");

    uint8_t* buffer_map;
    vkMapMemory(device.logical, shader_group_buffer_memory, 0, table_size, 0, (void**)&buffer_map);
    memcpy(buffer_map, shader_handle_data.data(), table_size);
    vkUnmapMemory(device.logical, shader_group_buffer_memory);
}

void RayTracer::create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding acceleration_structure_layout_binding{};
    acceleration_structure_layout_binding.binding = 0;
    acceleration_structure_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    acceleration_structure_layout_binding.descriptorCount = 1;
    acceleration_structure_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding result_image_layout_binding{};
    result_image_layout_binding.binding = 1;
    result_image_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    result_image_layout_binding.descriptorCount = 1;
    result_image_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding uniform_buffer_binding{};
    uniform_buffer_binding.binding = 2;
    uniform_buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_buffer_binding.descriptorCount = 1;
    uniform_buffer_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR; // TODO determine which stage descriptors are used in and set the bits accordingly

    VkDescriptorSetLayoutBinding vertex_binding{};
    vertex_binding.binding = 3;
    vertex_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertex_binding.descriptorCount = 1;
    vertex_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding index_binding{};
    index_binding.binding = 4;
    index_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    index_binding.descriptorCount = 1;
    index_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding object_binding{};
    object_binding.binding = 5;
    object_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    object_binding.descriptorCount = 1;
    object_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        acceleration_structure_layout_binding,
        result_image_layout_binding,
        uniform_buffer_binding,
        vertex_binding,
        index_binding,
        object_binding
    };

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device.logical, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout.");
}

void RayTracer::create_pipeline(Shader& ray_gen, Shader& ray_miss, Shader& ray_hit)
{
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;

    if (vkCreatePipelineLayout(device.logical, &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout.");

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    // Ray generation
    VkPipelineShaderStageCreateInfo ray_gen_stage = {};
    ray_gen_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ray_gen_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    ray_gen_stage.module = ray_gen.shader_module;
    ray_gen_stage.pName = "main";
    shader_stages.push_back(ray_gen_stage);
    VkRayTracingShaderGroupCreateInfoKHR ray_gen_group{};
    ray_gen_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    ray_gen_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    ray_gen_group.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
    ray_gen_group.closestHitShader = VK_SHADER_UNUSED_KHR;
    ray_gen_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    ray_gen_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(ray_gen_group);

    // Ray miss
    VkPipelineShaderStageCreateInfo miss_stage = {};
    miss_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    miss_stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    miss_stage.module = ray_miss.shader_module;
    miss_stage.pName = "main";
    shader_stages.push_back(miss_stage);
    VkRayTracingShaderGroupCreateInfoKHR miss_group{};
    miss_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    miss_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    miss_group.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
    miss_group.closestHitShader = VK_SHADER_UNUSED_KHR;
    miss_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    miss_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(miss_group);

    // Ray closest hit
    VkPipelineShaderStageCreateInfo closest_hit_stage = {};
    closest_hit_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    closest_hit_stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    closest_hit_stage.module = ray_hit.shader_module;
    closest_hit_stage.pName = "main";
    shader_stages.push_back(closest_hit_stage);
    VkRayTracingShaderGroupCreateInfoKHR closest_hit_group{};
    closest_hit_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    closest_hit_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closest_hit_group.generalShader = VK_SHADER_UNUSED_KHR;
    closest_hit_group.closestHitShader = static_cast<uint32_t>(shader_stages.size()) - 1;
    closest_hit_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    closest_hit_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(closest_hit_group);

    VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info{};
    raytracing_pipeline_create_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    raytracing_pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    raytracing_pipeline_create_info.pStages = shader_stages.data();
    raytracing_pipeline_create_info.groupCount = static_cast<uint32_t>(shader_groups.size());
    raytracing_pipeline_create_info.pGroups = shader_groups.data();
    raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = dispatcher.device.physical_device_info.ray_tracing_properties.maxRayRecursionDepth; // TODO
    raytracing_pipeline_create_info.layout = pipeline_layout;

    if (vkCreateRayTracingPipelinesKHR(device.logical, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ray tracing pipeline.");
}

static VkDescriptorBufferInfo create_buffer_descriptor(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0)
{
    VkDescriptorBufferInfo buffer_descriptor{};
    buffer_descriptor.buffer = buffer;
    buffer_descriptor.range = size;
    buffer_descriptor.offset = offset;
    return buffer_descriptor;
}

static VkWriteDescriptorSet write_buffer_descriptor(
    VkDescriptorSet descriptor_set,
    VkDescriptorBufferInfo& buffer_descriptor,
    VkDescriptorType type,
    uint32_t binding)
{
    VkWriteDescriptorSet buffer_write{};
    buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    buffer_write.dstSet = descriptor_set;
    buffer_write.descriptorType = type;
    buffer_write.dstBinding = binding;
    buffer_write.pBufferInfo = &buffer_descriptor;
    buffer_write.descriptorCount = 1;
    return buffer_write;
}

void RayTracer::create_descriptor_sets(Dispatcher& dispatch)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = dispatch.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout;

    if (vkAllocateDescriptorSets(device.logical, &alloc_info, &descriptor_set) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set.");

    VkWriteDescriptorSetAccelerationStructureKHR descriptor_acceleration_structure_info{};
    descriptor_acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptor_acceleration_structure_info.accelerationStructureCount = 1;
    descriptor_acceleration_structure_info.pAccelerationStructures = &tlas;

    VkWriteDescriptorSet acceleration_structure_write{};
    acceleration_structure_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    acceleration_structure_write.dstSet = descriptor_set;
    acceleration_structure_write.dstBinding = 0;
    acceleration_structure_write.descriptorCount = 1;
    acceleration_structure_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    acceleration_structure_write.pNext = &descriptor_acceleration_structure_info;

    VkDescriptorImageInfo dest_image_descriptor{}; // TODO make helper function for image descriptor info and write
    dest_image_descriptor.imageView = dest_image_view;
    dest_image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo ubo_descriptor = create_buffer_descriptor(uniform_buffer, sizeof(UniformBufferObject));
    VkDescriptorBufferInfo vertex_descriptor = create_buffer_descriptor(vertex_buffer, vertex_end_indices.back() * sizeof(Vertex));
    VkDescriptorBufferInfo index_descriptor = create_buffer_descriptor(index_buffer, index_end_indices.back() * sizeof(uint32_t));
    VkDescriptorBufferInfo object_descriptor = create_buffer_descriptor(object_buffer, scene->get_object_variants().size() * sizeof(ObjectData));

    VkWriteDescriptorSet result_image_write{};
    result_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    result_image_write.dstSet = descriptor_set;
    result_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    result_image_write.dstBinding = 1;
    result_image_write.pImageInfo = &dest_image_descriptor;
    result_image_write.descriptorCount = 1;

    VkWriteDescriptorSet uniform_buffer_write = write_buffer_descriptor(descriptor_set, ubo_descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
    VkWriteDescriptorSet vertex_buffer_write = write_buffer_descriptor(descriptor_set, vertex_descriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3);
    VkWriteDescriptorSet index_buffer_write = write_buffer_descriptor(descriptor_set, index_descriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
    VkWriteDescriptorSet object_buffer_write = write_buffer_descriptor(descriptor_set, object_descriptor, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5);

    const std::array<VkWriteDescriptorSet, 6> write_descriptor_sets = {
        acceleration_structure_write,
        result_image_write,
        uniform_buffer_write,
        vertex_buffer_write,
        index_buffer_write,
        object_buffer_write
    };
    vkUpdateDescriptorSets(device.logical, static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, VK_NULL_HANDLE);
}

void RayTracer::create_command_buffer(Dispatcher& dispatch) // TODO move command buffer handling outside of path tracer and rasteriser
{
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool = dispatch.command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device.logical, &cmd_buffer_alloc_info, &command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers.");
}

void RayTracer::create_sync_objects()
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device.logical, &semaphore_create_info, nullptr, &render_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(device.logical, &fence_create_info, nullptr, &render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create fence.");
}

void RayTracer::free_scene_buffers()
{
    assert(scene != nullptr);

    vkDestroyBuffer(device.logical, vertex_buffer, nullptr);
    vkFreeMemory(device.logical, vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, index_buffer, nullptr);
    vkFreeMemory(device.logical, index_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, object_buffer, nullptr);
    vkFreeMemory(device.logical, object_buffer_memory, nullptr);

    vkDestroyBuffer(device.logical, tlas_buffer, nullptr);
    vkFreeMemory(device.logical, tlas_memory, nullptr);
    vkDestroyBuffer(device.logical, blas_buffer, nullptr);
    vkFreeMemory(device.logical, blas_memory, nullptr);
    vkDestroyAccelerationStructureKHR(device.logical, tlas, nullptr);
    for (auto b : blas)
        vkDestroyAccelerationStructureKHR(device.logical, b, nullptr);
}

static VkDeviceAddress get_buffer_address(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = buffer;

    return vkGetBufferDeviceAddress(device, &address_info);
}

void RayTracer::set_scene(Dispatcher& dispatcher, const Scene& scene)
{
    // TODO unify the buffer creations with the rasteriser.

    if (this->scene != nullptr)
        free_scene_buffers();

    this->scene = &scene;

    size_t vertex_count = 0, tri_count = 0;
    const std::vector<ObjectVariant>& object_variants = scene.get_object_variants();

    for (const ObjectVariant& variant : object_variants) {
        vertex_count += variant.mesh.get_vertices().size();
        tri_count += variant.mesh.get_indexed_triangles().size();
    }

    Vertex* all_vertices = new Vertex[vertex_count];
    uint32_t* all_indices = new uint32_t[tri_count * 3];
    ObjectData* all_object_data = new ObjectData[object_variants.size()];

    vertex_end_indices.resize(object_variants.size());
    index_end_indices.resize(object_variants.size());

    uint32_t vertex_count_acc = 0, index_count_acc = 0;

    for (size_t i = 0; i < object_variants.size(); i++) {
        all_object_data[i].vertex_index = vertex_count_acc;
        all_object_data[i].index_index = index_count_acc;
        all_object_data[i].material_index = 0;

        const ObjectVariant& variant = object_variants[i];

        memcpy(all_vertices + vertex_count_acc, variant.mesh.get_vertices().data(), variant.mesh.get_vertices().size() * sizeof(Vertex));
        memcpy(all_indices + index_count_acc, variant.mesh.get_indexed_triangles().data(), variant.mesh.get_indexed_triangles().size() * sizeof(IndexedTriangle));

        vertex_count_acc += variant.mesh.get_vertices().size();
        index_count_acc += variant.mesh.get_indexed_triangles().size() * 3;

        vertex_end_indices[i] = vertex_count_acc;
        index_end_indices[i] = index_count_acc;
    }

    assert(vertex_end_indices.back() == vertex_count);
    assert(index_end_indices.back() == tri_count * 3);

    auto common_buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    device.create_buffer(vertex_buffer,
                         vertex_buffer_memory,
                         vertex_count * sizeof(Vertex),
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | common_buffer_usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    dispatcher.transfer_to_buffer(vertex_buffer,
                                  all_vertices,
                                  vertex_count * sizeof(Vertex));
    vertex_buffer_address = get_buffer_address(device.logical, vertex_buffer);

    device.create_buffer(index_buffer,
                         index_buffer_memory,
                         tri_count * 3 * sizeof(uint32_t),
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | common_buffer_usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    dispatcher.transfer_to_buffer(index_buffer,
                                  all_indices,
                                  tri_count * 3 * sizeof(uint32_t));
    index_buffer_address = get_buffer_address(device.logical, index_buffer);

    device.create_buffer(object_buffer,
                         object_buffer_memory,
                         object_variants.size() * sizeof(ObjectData),
                         common_buffer_usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    dispatcher.transfer_to_buffer(object_buffer,
                                  all_object_data,
                                  object_variants.size() * sizeof(ObjectData));

    delete[] all_vertices;
    delete[] all_indices;
    delete[] all_object_data;

    create_blas(dispatcher);
    create_tlas(dispatcher);
}

void RayTracer::set_camera(Dispatcher& dispatcher, const Camera& camera)
{
    assert(scene != nullptr);

    Mat4 view = camera.view_matrix();
    Mat4 proj = camera.projection_matrix();

    uniform_map->inv_proj = glm::inverse(proj);
    uniform_map->inv_view = glm::inverse(view);
    uniform_map->near = camera.near;
    uniform_map->far = camera.far;

    samples_taken = 0;
}

void RayTracer::set_samples(uint32_t samples)
{
    // TODO check if in render. But begin and end render should be merged, so this will not be a problem.
    if (samples == 0)
        throw std::runtime_error("Samples was zero.");
    uniform_map->samples = samples;
}

void RayTracer::set_max_bounces(uint32_t max_bounces)
{
    samples_taken = 0;
    uniform_map->max_bounces = max_bounces;
}

void RayTracer::create_blas(Dispatcher& dispatcher)
{
    assert(scene != nullptr);

    const size_t blas_count = scene->get_object_variants().size();

    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> range_infos;
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> size_infos;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> range_info_ptrs;
    geometries.resize(blas_count);
    range_infos.resize(blas_count);
    size_infos.resize(blas_count);
    range_info_ptrs.resize(blas_count);

    VkDeviceSize total_blas_size = 0;
    VkDeviceSize max_scratch_size = 0;

    for (size_t i = 0; i < blas_count; i++) {

        const ObjectVariant& object = scene->get_object_variants()[i];

        VkDeviceOrHostAddressConstKHR vertex_address_const{ .deviceAddress = vertex_buffer_address + ((i == 0) ? 0 : (vertex_end_indices[i - 1] * sizeof(Vertex))) };
        VkDeviceOrHostAddressConstKHR index_address_const{ .deviceAddress = index_buffer_address + ((i == 0) ? 0 : (index_end_indices[i - 1] * sizeof(uint32_t))) };

        VkAccelerationStructureGeometryKHR& geometry = geometries[i];
        geometry = {};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData = vertex_address_const;
        geometry.geometry.triangles.maxVertex = object.mesh.get_vertices().size() - 1;
        geometry.geometry.triangles.vertexStride = sizeof(Vertex);
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.indexData = index_address_const;
        // geometry.geometry.triangles.transformData = ...;

        VkAccelerationStructureBuildRangeInfoKHR& range = range_infos[i];
        range = {};
        range.primitiveCount = object.mesh.get_indexed_triangles().size();
        range.primitiveOffset = 0;
        range.firstVertex = 0;
        range.transformOffset = 0;
        range_info_ptrs[i] = &range_infos[i];

        VkAccelerationStructureBuildGeometryInfoKHR acceleration_structure_build_geometry_info{};
        acceleration_structure_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        acceleration_structure_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acceleration_structure_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        acceleration_structure_build_geometry_info.geometryCount = 1;
        acceleration_structure_build_geometry_info.pGeometries = &geometry;

        const uint32_t max_primitive_count = static_cast<uint32_t>(object.mesh.get_indexed_triangles().size());

        VkAccelerationStructureBuildSizesInfoKHR& size = size_infos[i];
        size = {};
        size.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.logical,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &acceleration_structure_build_geometry_info,
            &max_primitive_count,
            &size);

        total_blas_size += round_up_to<VkDeviceSize>(size.accelerationStructureSize, 256);
        max_scratch_size = std::max(max_scratch_size, size.buildScratchSize);
    }

    device.create_buffer(blas_buffer,
                         blas_memory,
                         total_blas_size,
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    blas.resize(blas_count);
    create_acceleration_structure(
        blas.data(),
        dispatcher,
        blas_buffer,
        size_infos.data(),
        geometries.data(),
        range_info_ptrs.data(),
        max_scratch_size,
        blas_count,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
}

static VkTransformMatrixKHR to_vk_transform_matrix(const Mat4& mat)
{
    VkTransformMatrixKHR vkTransformMatrix = {};

    vkTransformMatrix.matrix[0][0] = mat[0][0];
    vkTransformMatrix.matrix[0][1] = mat[0][1];
    vkTransformMatrix.matrix[0][2] = mat[0][2];
    vkTransformMatrix.matrix[1][0] = mat[1][0];
    vkTransformMatrix.matrix[1][1] = mat[1][1];
    vkTransformMatrix.matrix[1][2] = mat[1][2];
    vkTransformMatrix.matrix[2][0] = mat[2][0];
    vkTransformMatrix.matrix[2][1] = mat[2][1];
    vkTransformMatrix.matrix[2][2] = mat[2][2];

    return vkTransformMatrix;
}

void RayTracer::create_tlas(Dispatcher& dispatcher)
{
    assert(scene != nullptr);
    assert(blas.size() == scene->get_object_variants().size());

    size_t total_instance_count = 0;
    for (const ObjectVariant& object_variant : scene->get_object_variants())
        total_instance_count += object_variant.instances.size();

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(total_instance_count);

    for (size_t i = 0; i < scene->get_object_variants().size(); i++) {
        VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
        acceleration_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        acceleration_device_address_info.accelerationStructure = blas[i];
        VkDeviceAddress blas_address = vkGetAccelerationStructureDeviceAddressKHR(device.logical, &acceleration_device_address_info);

        const ObjectVariant& object_variant = scene->get_object_variants()[i];

        for (size_t j = 0; j < object_variant.instances.size(); j++) {
            instances.emplace_back();
            VkAccelerationStructureInstanceKHR& acceleration_structure_instance = instances.back();
            acceleration_structure_instance = {};
            acceleration_structure_instance.transform = to_vk_transform_matrix(object_variant.instances[j].transform.matrix);
            acceleration_structure_instance.instanceCustomIndex = i;
            acceleration_structure_instance.mask = 0xFF;
            acceleration_structure_instance.instanceShaderBindingTableRecordOffset = 0;
            acceleration_structure_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            acceleration_structure_instance.accelerationStructureReference = blas_address;
        }
    }

    assert(instances.size() == total_instance_count);

    VkBuffer instance_buffer;
    VkDeviceMemory instance_buffer_memory;

    device.create_buffer(instance_buffer,
                         instance_buffer_memory,
                         instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    dispatcher.transfer_to_buffer(instance_buffer,
                                  instances.data(),
                                  instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
    VkDeviceOrHostAddressConstKHR instance_buffer_address = { .deviceAddress = get_buffer_address(device.logical, instance_buffer) };

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data = instance_buffer_address;

    // Get the size requirements for buffers involved in the acceleration structure build process
    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
    build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_geometry_info.geometryCount = 1;
    build_geometry_info.pGeometries = &geometry;

    const uint32_t primitive_count = static_cast<uint32_t>(total_instance_count);

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{};
    build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        device.logical, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_geometry_info, &primitive_count, &build_sizes_info);

    VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
    build_range_info.primitiveCount = primitive_count;
    build_range_info.primitiveOffset = 0;
    build_range_info.firstVertex = 0;
    build_range_info.transformOffset = 0;

    device.create_buffer(tlas_buffer,
                         tlas_memory,
                         build_sizes_info.accelerationStructureSize,
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureBuildRangeInfoKHR* build_range_info_ptr = &build_range_info;
    create_acceleration_structure(&tlas,
                                  dispatcher,
                                  tlas_buffer,
                                  &build_sizes_info,
                                  &geometry,
                                  &build_range_info_ptr,
                                  build_sizes_info.buildScratchSize,
                                  1,
                                  VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

    vkFreeMemory(device.logical, instance_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, instance_buffer, nullptr);
}

void RayTracer::create_acceleration_structure(
    VkAccelerationStructureKHR* out,
    Dispatcher& dispatcher,
    VkBuffer acc_struct_buffer,
    const VkAccelerationStructureBuildSizesInfoKHR* size_infos,
    const VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* const* range_info_ptrs,
    VkDeviceSize max_scratch_size,
    const size_t count,
    VkAccelerationStructureTypeKHR type)
{
    VkBuffer scratch_buffer;
    VkDeviceMemory scratch_buffer_memory;
    device.create_buffer(scratch_buffer,
                         scratch_buffer_memory,
                         max_scratch_size,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
    VkDeviceAddress scratch_buffer_address = get_buffer_address(device.logical, scratch_buffer);

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_geometry_infos;
    build_geometry_infos.resize(count);
    VkDeviceSize acc_struct_offset = 0;

    for (size_t i = 0; i < count; i++) {

        VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
        acceleration_structure_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        acceleration_structure_create_info.buffer = acc_struct_buffer;
        acceleration_structure_create_info.size = size_infos[i].accelerationStructureSize;
        acceleration_structure_create_info.type = type;
        acceleration_structure_create_info.offset = acc_struct_offset;
        vkCreateAccelerationStructureKHR(device.logical, &acceleration_structure_create_info, nullptr, &out[i]);

        acc_struct_offset += round_up_to<VkDeviceSize>(size_infos[i].accelerationStructureSize, 256);

        VkAccelerationStructureBuildGeometryInfoKHR& build_geometry = build_geometry_infos[i];
        build_geometry = {};
        build_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_geometry.type = type;
        build_geometry.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build_geometry.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_geometry.dstAccelerationStructure = out[i];
        build_geometry.geometryCount = 1;
        build_geometry.pGeometries = &geometries[i];
        build_geometry.scratchData.deviceAddress = scratch_buffer_address;
    }

    VkCommandBuffer command_buffer = dispatcher.begin_one_time_use_command_buffer();

    vkCmdBuildAccelerationStructuresKHR(
        command_buffer,
        count,
        build_geometry_infos.data(),
        range_info_ptrs);

    dispatcher.end_one_time_use_command_buffer(command_buffer, device.graphics_queue);

    vkFreeMemory(device.logical, scratch_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, scratch_buffer, nullptr);
}

void RayTracer::write_command_buffer()
{
    const uint32_t aligned_handle_size = round_up_to<uint32_t>(
        device.physical_device_info.ray_tracing_properties.shaderGroupHandleSize,
        device.physical_device_info.ray_tracing_properties.shaderGroupHandleAlignment);

    const VkDeviceSize shader_group_buffer_address = get_buffer_address(device.logical, shader_group_buffer);

    VkStridedDeviceAddressRegionKHR ray_gen_sbt{};
    ray_gen_sbt.deviceAddress = shader_group_buffer_address + aligned_handle_size * 0;
    ray_gen_sbt.stride = aligned_handle_size;
    ray_gen_sbt.size = aligned_handle_size;

    VkStridedDeviceAddressRegionKHR miss_sbt{};
    miss_sbt.deviceAddress = shader_group_buffer_address + aligned_handle_size * 1;
    miss_sbt.stride = aligned_handle_size;
    miss_sbt.size = aligned_handle_size;

    VkStridedDeviceAddressRegionKHR hit_sbt{};
    hit_sbt.deviceAddress = shader_group_buffer_address + aligned_handle_size * 2;
    hit_sbt.stride = aligned_handle_size;
    hit_sbt.size = aligned_handle_size;

    // std::vector<VkBufferMemoryBarrier> barriers;

    // auto buffer_barrier = [](VkBuffer buffer, VkDeviceSize size) {
    //     VkBufferMemoryBarrier barrier{};
    //     barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     barrier.buffer = buffer;
    //     barrier.size = size;
    //     return barrier;
    // };

    // barriers.emplace_back(buffer_barrier(object_buffer, scene->get_object_variants().size() * sizeof(ObjectData)));
    // barriers.emplace_back(buffer_barrier(uniform_buffer, sizeof(UniformBufferObject)));

    // vkCmdPipelineBarrier(
    //     command_buffer,
    //     VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
    //     VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_HOST_BIT,
    //     0,
    //     // Memory
    //     0,
    //     VK_NULL_HANDLE,
    //     // Buffers
    //     static_cast<uint32_t>(barriers.size()),
    //     barriers.data(),
    //     // Images
    //     0,
    //     VK_NULL_HANDLE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

    VkStridedDeviceAddressRegionKHR callable_sbt{};

    vkCmdTraceRaysKHR(
        command_buffer,
        &ray_gen_sbt,
        &miss_sbt,
        &hit_sbt,
        &callable_sbt,
        extent.width,
        extent.height,
        1);
}

void RayTracer::wait_for_render()
{
    vkWaitForFences(device.logical, 1, &render_fence, VK_TRUE, UINT64_MAX);
}

void RayTracer::begin_render()
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");
    if (in_render)
        throw std::runtime_error("Ray tracer is already rendering.");

    uniform_map->new_samples_mult = (float)uniform_map->samples / (samples_taken + uniform_map->samples);
    uniform_map->old_samples_mult = (float)samples_taken / (samples_taken + uniform_map->samples);
    uniform_map->seed = Uint4(rng.next(), rng.next(), rng.next(), rng.next());

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    // TODO avoid writing each frame (implement together with frames in flight)
    write_command_buffer();

    in_render = true;
}

VkSemaphore RayTracer::end_render(VkSemaphore* wait_for, uint32_t semaphore_count)
{
    if (!in_render)
        throw std::runtime_error("Render ended before having begun.");

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = semaphore_count;
    submit_info.pWaitSemaphores = wait_for;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device.logical, 1, &render_fence);

    if (vkQueueSubmit(device.graphics_queue, 1, &submit_info, render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    samples_taken += uniform_map->samples;
    in_render = false;
    return render_semaphore;
}

void RayTracer::set_extent(uint32_t width, uint32_t height)
{
    extent.width = width;
    extent.height = height;
    destroy_dest_image();
    create_dest_image(dispatcher);

    VkDescriptorImageInfo image_descriptor{};
    image_descriptor.imageView = dest_image_view;
    image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstSet = descriptor_set;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write_descriptor_set.dstBinding = 1;
    write_descriptor_set.pImageInfo = &image_descriptor;
    write_descriptor_set.descriptorCount = 1;
    vkUpdateDescriptorSets(device.logical, 1, &write_descriptor_set, 0, VK_NULL_HANDLE);
}

void RayTracer::copy_result(VkImage image)
{
    VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    transition_image_layout(command_buffer,
                            image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            dest_image,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    VkImageCopy copy_region{};
    copy_region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy_region.srcOffset = { 0, 0, 0 };
    copy_region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy_region.dstOffset = { 0, 0, 0 };
    copy_region.extent = { extent.width, extent.height, 1 };
    vkCmdCopyImage(command_buffer, dest_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    transition_image_layout(command_buffer,
                            image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            dest_image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            subresource_range);
}