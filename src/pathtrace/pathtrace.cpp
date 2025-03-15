#include "pathtrace.h"

#include <cassert>

std::vector<VkDescriptorPoolSize> PathTracer::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
             { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 },
             { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
}

static std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layout_bindings()
{
    // TODO VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR depends on whether it is recursive
    VkDescriptorSetLayoutBinding acceleration_structure_layout_binding = DescriptorSetLayout::create_layout_binding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

    VkDescriptorSetLayoutBinding result_image_layout_binding = DescriptorSetLayout::create_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    VkDescriptorSetLayoutBinding uniform_buffer_binding = DescriptorSetLayout::create_layout_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

    VkDescriptorSetLayoutBinding vertex_binding = DescriptorSetLayout::create_layout_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

    VkDescriptorSetLayoutBinding index_binding = DescriptorSetLayout::create_layout_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

    VkDescriptorSetLayoutBinding object_binding = DescriptorSetLayout::create_layout_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

    return {
        acceleration_structure_layout_binding,
        result_image_layout_binding,
        uniform_buffer_binding,
        vertex_binding,
        index_binding,
        object_binding
    };
}

constexpr VkBufferUsageFlagBits scene_buffer_usage = VkBufferUsageFlagBits();

PathTracer::PathTracer(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    VkExtent2D extent)
    : device(device)
    , extent(extent)
    , scene_buffer(device, command_pool, scene, VkBufferUsageFlagBits(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR), VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)
    , acceleration_structure(device, command_pool, scene_buffer)
    , descriptor_set_layout(device, get_descriptor_set_layout_bindings())
    , descriptor_set(descriptor_pool, descriptor_set_layout)
    , in_render(false)
{
    Splitmix32 sm(1);
    rng = Xshiro128(sm.next(), sm.next(), sm.next(), sm.next());

    create_dest_image(command_pool);
    create_pipeline();
    create_shader_binding_tables();

    device.create_buffer(uniform_buffer, uniform_buffer_memory, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(device.logical_handle(), uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, (void**)&uniform_map);

    set_scene(command_pool, scene);

    set_samples(1);
    set_max_bounces(1);
}

PathTracer::~PathTracer()
{
    vkDestroyBuffer(device.logical_handle(), uniform_buffer, nullptr);
    vkUnmapMemory(device.logical_handle(), uniform_buffer_memory);
    vkFreeMemory(device.logical_handle(), uniform_buffer_memory, nullptr);

    destroy_dest_image();

    vkDestroyPipeline(device.logical_handle(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical_handle(), pipeline_layout, nullptr);

    vkDestroyBuffer(device.logical_handle(), shader_group_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), shader_group_buffer_memory, nullptr);
}

void PathTracer::create_dest_image(CommandPool& command_pool)
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

    VkCommandBuffer command_buffer = command_pool.begin_one_time_use_command_buffer();

    transition_image_layout(command_buffer,
                            dest_image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL,
                            0,
                            0,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    command_pool.end_one_time_use_command_buffer(command_buffer, device.get_graphics_queue());
}

void PathTracer::destroy_dest_image()
{
    vkDestroyImageView(device.logical_handle(), dest_image_view, nullptr);
    vkDestroyImage(device.logical_handle(), dest_image, nullptr);
    vkFreeMemory(device.logical_handle(), dest_image_memory, nullptr);
}

void PathTracer::create_shader_binding_tables()
{
    const uint32_t aligned_handle_size = round_up_to<uint32_t>(
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleSize,
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleAlignment);
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
    if (vkGetRayTracingShaderGroupHandlesKHR(device.logical_handle(), pipeline, 0, group_count, table_size, shader_handle_data.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to get ray tracing shader group handles.");

    uint8_t* buffer_map;
    vkMapMemory(device.logical_handle(), shader_group_buffer_memory, 0, table_size, 0, (void**)&buffer_map);
    memcpy(buffer_map, shader_handle_data.data(), table_size);
    vkUnmapMemory(device.logical_handle(), shader_group_buffer_memory);
}

void PathTracer::create_pipeline()
{
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout.handle();

    if (vkCreatePipelineLayout(device.logical_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout.");

    Shader ray_gen(device, "bin/shaders/pathtrace/pathtrace.rg.spv");
    Shader ray_miss(device, "bin/shaders/pathtrace/pathtrace.ms.spv");
    Shader ray_closest_hit(device, "bin/shaders/pathtrace/pathtrace.ch.spv");

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    // Ray generation
    VkPipelineShaderStageCreateInfo ray_gen_stage = {};
    ray_gen_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ray_gen_stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    ray_gen_stage.module = ray_gen.handle();
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
    miss_stage.module = ray_miss.handle();
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
    closest_hit_stage.module = ray_closest_hit.handle();
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
    raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = device.get_physical_device_info().ray_tracing_properties.maxRayRecursionDepth; // TODO
    raytracing_pipeline_create_info.layout = pipeline_layout;

    if (vkCreateRayTracingPipelinesKHR(device.logical_handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ray tracing pipeline.");
}

void PathTracer::update_descriptor_sets()
{
    VkDescriptorImageInfo dest_image_descriptor = DescriptorSet::create_descriptor(dest_image_view, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorBufferInfo ubo_descriptor = DescriptorSet::create_descriptor(uniform_buffer, sizeof(UniformBufferObject));
    VkDescriptorBufferInfo vertex_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.vertex_region_size(), scene_buffer.get_vertex_offset());
    VkDescriptorBufferInfo index_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.index_region_size(), scene_buffer.get_index_offset());
    VkDescriptorBufferInfo object_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.instance_region_size(), scene_buffer.get_instance_offset());

    VkWriteDescriptorSetAccelerationStructureKHR descriptor_set_acceleration_structure_info;
    VkWriteDescriptorSet acceleration_structure_write = descriptor_set.write_descriptor_set(acceleration_structure.get_top_level(), descriptor_set_acceleration_structure_info, 0);
    VkWriteDescriptorSet result_image_write = descriptor_set.write_descriptor_set(dest_image_descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    VkWriteDescriptorSet uniform_buffer_write = descriptor_set.write_descriptor_set(ubo_descriptor, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkWriteDescriptorSet vertex_buffer_write = descriptor_set.write_descriptor_set(vertex_descriptor, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    VkWriteDescriptorSet index_buffer_write = descriptor_set.write_descriptor_set(index_descriptor, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    VkWriteDescriptorSet object_buffer_write = descriptor_set.write_descriptor_set(object_descriptor, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    const std::array<VkWriteDescriptorSet, 6> write_descriptor_sets = {
        acceleration_structure_write,
        result_image_write,
        uniform_buffer_write,
        vertex_buffer_write,
        index_buffer_write,
        object_buffer_write
    };

    DescriptorSet::update_write_descriptors(device, write_descriptor_sets.data(), static_cast<uint32_t>(write_descriptor_sets.size()));
}

void PathTracer::set_scene(CommandPool& command_pool, const Scene& scene)
{
    scene_buffer.rebuild(command_pool, scene);
    acceleration_structure.rebuild(command_pool, scene_buffer);

    update_descriptor_sets();
}

void PathTracer::set_camera(CommandPool& command_pool, const Camera& camera)
{
    Mat4 view = camera.view_matrix();
    Mat4 proj = camera.projection_matrix();

    uniform_map->inv_proj = glm::inverse(proj);
    uniform_map->inv_view = glm::inverse(view);
    uniform_map->near = camera.near;
    uniform_map->far = camera.far;

    samples_taken = 0;
}

void PathTracer::set_samples(uint32_t samples)
{
    if (samples == 0)
        throw std::runtime_error("Samples was zero.");
    uniform_map->samples = samples;
}

void PathTracer::set_max_bounces(uint32_t max_bounces)
{
    samples_taken = 0;
    uniform_map->max_bounces = max_bounces;
}

void PathTracer::write_command_buffer(VkCommandBuffer command_buffer)
{
    const uint32_t aligned_handle_size = round_up_to<uint32_t>(
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleSize,
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleAlignment);

    const VkDeviceSize shader_group_buffer_address = device.get_buffer_address(shader_group_buffer);

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

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_set.handle(), 0, nullptr);

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

void PathTracer::set_extent(CommandPool& command_pool, uint32_t width, uint32_t height)
{
    extent.width = width;
    extent.height = height;
    destroy_dest_image();
    create_dest_image(command_pool);

    VkDescriptorImageInfo image_descriptor = DescriptorSet::create_descriptor(dest_image_view, VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet write_descriptor_set = descriptor_set.write_descriptor_set(image_descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    DescriptorSet::update_write_descriptors(device, &write_descriptor_set, 1);
}

void PathTracer::update_uniforms()
{
    uniform_map->new_samples_mult = (float)uniform_map->samples / (samples_taken + uniform_map->samples);
    uniform_map->old_samples_mult = (float)samples_taken / (samples_taken + uniform_map->samples);
    uniform_map->seed = Uint4(rng.next(), rng.next(), rng.next(), rng.next());
    samples_taken += uniform_map->samples;
}
