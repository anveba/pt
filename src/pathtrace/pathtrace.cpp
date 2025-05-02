#include "pathtrace.h"

#include <cassert>

std::vector<VkDescriptorPoolSize> PathTracer::get_descriptor_pool_sizes()
{
    return {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1 * IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_COUNT * IN_FLIGHT }
    };
}

constexpr size_t DESCRIPTOR_BINDING_COUNT = 11;

static inline void get_shader_group_alignments(
    const Device& device,
    uint32_t& aligned_handle_size,
    uint32_t& base_alignment)
{
    aligned_handle_size = round_up_to<uint32_t>(
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleSize,
        device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleAlignment);
    base_alignment = device.get_physical_device_info().ray_tracing_properties.shaderGroupBaseAlignment;
}

constexpr size_t SHADER_BINDING_TABLE_ENTRY_COUNTS[4] = { 1, 2, 1, 0 }; // Ray gen, miss, hit, callable

static std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layout_bindings(const VkSampler& sampler)
{
    VkDescriptorSetLayoutBinding acceleration_structure_layout_binding = DescriptorSetLayout::create_layout_binding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding result_image_layout_binding = DescriptorSetLayout::create_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    VkDescriptorSetLayoutBinding uniform_buffer_binding = DescriptorSetLayout::create_layout_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding vertex_binding = DescriptorSetLayout::create_layout_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding index_binding = DescriptorSetLayout::create_layout_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding object_binding = DescriptorSetLayout::create_layout_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding material_binding = DescriptorSetLayout::create_layout_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding texture_sampler_binding = DescriptorSetLayout::create_layout_binding(7, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, &sampler);
    VkDescriptorSetLayoutBinding texture_binding = DescriptorSetLayout::create_layout_binding(8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, MAX_TEXTURE_COUNT);
    VkDescriptorSetLayoutBinding light_sampler_binding = DescriptorSetLayout::create_layout_binding(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    VkDescriptorSetLayoutBinding queue_binding = DescriptorSetLayout::create_layout_binding(10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        acceleration_structure_layout_binding,
        result_image_layout_binding,
        uniform_buffer_binding,
        vertex_binding,
        index_binding,
        object_binding,
        material_binding,
        texture_sampler_binding,
        texture_binding,
        light_sampler_binding,
        queue_binding
    };
    assert(bindings.size() == DESCRIPTOR_BINDING_COUNT);
    return bindings;
}

static std::vector<VkDescriptorBindingFlags> get_descriptor_binding_flags()
{
    std::vector<VkDescriptorBindingFlags> flags;
    flags.resize(DESCRIPTOR_BINDING_COUNT);
    for (uint32_t i = 0; i < DESCRIPTOR_BINDING_COUNT; i++)
        flags[i] = 0;
    flags[8] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    return flags;
}

PathTracer::PathTracer(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    StorageImage& accumulation_image)
    : device(device)
    , scene_buffer(device, command_pool, scene, VkBufferUsageFlagBits(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR), VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT)
    , scene_textures(device, command_pool, scene, 0)
    , acceleration_structure(device, command_pool, scene, scene_buffer)
    , wavefront_queue(device, 512 * 512)
    , uniform_buffers(device, sizeof(PathTraceUniformData) * IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    , texture_sampler(device)
    , descriptor_set_layout(device, get_descriptor_set_layout_bindings(texture_sampler.handle()), get_descriptor_binding_flags().data())
    , descriptor_sets(descriptor_pool, descriptor_set_layout)
    , resetter(device, descriptor_pool, command_pool, Shader(device, "bin/shaders/pathtrace/ptreset.cs.spv"), descriptor_set_layout, descriptor_sets, Uint3(1))
    , queue_swapper(device, descriptor_pool, command_pool, Shader(device, "bin/shaders/pathtrace/swapqueue.cs.spv"), descriptor_set_layout, descriptor_sets, Uint3(1))
{
    create_pipeline();
    create_shader_binding_tables();

    update_scene_uniform_data(scene);
    update_descriptor_sets();
    set_accumulation_image(command_pool, accumulation_image);

    uniform_data.sample_index = 0;
    uniform_data.queue_capacity = wavefront_queue.get_queue_max_count();

    set_samples(1);
    set_max_bounces(1);
}

PathTracer::~PathTracer()
{
    vkDestroyPipeline(device.logical_handle(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical_handle(), pipeline_layout, nullptr);

    vkDestroyBuffer(device.logical_handle(), shader_group_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), shader_group_buffer_memory, nullptr);
}

void PathTracer::create_shader_binding_tables()
{
    uint32_t handle_size = device.get_physical_device_info().ray_tracing_properties.shaderGroupHandleSize;
    uint32_t aligned_handle_size, base_alignment;
    get_shader_group_alignments(device, aligned_handle_size, base_alignment);

    uint32_t buffer_size = 0;
    uint32_t handle_count = 0;

    for (size_t count : SHADER_BINDING_TABLE_ENTRY_COUNTS) {
        buffer_size += aligned_handle_size * count;
        buffer_size = round_up_to<uint32_t>(buffer_size, base_alignment);
        handle_count += count;
    }

    assert(handle_count == shader_groups.size());

    device.create_buffer(
        shader_group_buffer,
        shader_group_buffer_memory,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    const uint32_t table_size = handle_size * handle_count;
    std::vector<uint8_t> shader_handle_data;
    shader_handle_data.resize(table_size);
    if (vkGetRayTracingShaderGroupHandlesKHR(device.logical_handle(), pipeline, 0, handle_count, table_size, shader_handle_data.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to get ray tracing shader group handles.");

    uint8_t* buffer_map;
    vkMapMemory(device.logical_handle(), shader_group_buffer_memory, 0, table_size, 0, (void**)&buffer_map);

    size_t data_back = 0;
    size_t buffer_map_back = 0;
    for (size_t count : SHADER_BINDING_TABLE_ENTRY_COUNTS) {

        for (size_t i = 0; i < count; i++) {
            memcpy(buffer_map + buffer_map_back, shader_handle_data.data() + data_back, handle_size);
            buffer_map_back += aligned_handle_size;
            data_back += handle_size;
        }

        buffer_map_back = round_up_to<size_t>(buffer_map_back, base_alignment);
    }

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

    Shader ray_gen(device, USE_WAVEFRONT ? "bin/shaders/pathtrace/pathtrace_wf.rg.spv" : "bin/shaders/pathtrace/pathtrace.rg.spv");
    Shader ray_miss(device, "bin/shaders/pathtrace/pathtrace.ms.spv");
    Shader shadow_ray_miss(device, "bin/shaders/pathtrace/shadowray.ms.spv");
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

    // Shadow ray miss
    VkPipelineShaderStageCreateInfo shadow_ray_miss_stage = {};
    shadow_ray_miss_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shadow_ray_miss_stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shadow_ray_miss_stage.module = shadow_ray_miss.handle();
    shadow_ray_miss_stage.pName = "main";
    shader_stages.push_back(shadow_ray_miss_stage);
    VkRayTracingShaderGroupCreateInfoKHR shadow_ray_miss_group{};
    shadow_ray_miss_group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shadow_ray_miss_group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shadow_ray_miss_group.generalShader = static_cast<uint32_t>(shader_stages.size()) - 1;
    shadow_ray_miss_group.closestHitShader = VK_SHADER_UNUSED_KHR;
    shadow_ray_miss_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    shadow_ray_miss_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(shadow_ray_miss_group);

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
    raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = 2;
    if (device.get_physical_device_info().ray_tracing_properties.maxRayRecursionDepth < raytracing_pipeline_create_info.maxPipelineRayRecursionDepth)
        throw std::runtime_error("Device does not support a recursion depth of " + std::to_string(raytracing_pipeline_create_info.maxPipelineRayRecursionDepth) + ".");
    raytracing_pipeline_create_info.layout = pipeline_layout;

    if (vkCreateRayTracingPipelinesKHR(device.logical_handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ray tracing pipeline.");
}

void PathTracer::update_descriptor_sets()
{
    VkDescriptorBufferInfo vertex_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.vertex_region_size(), scene_buffer.get_vertex_offset());
    VkDescriptorBufferInfo index_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.index_region_size(), scene_buffer.get_index_offset());
    VkDescriptorBufferInfo object_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.instance_region_size(), scene_buffer.get_instance_offset());
    VkDescriptorBufferInfo material_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.material_region_size(), scene_buffer.get_material_offset());
    VkDescriptorBufferInfo light_sampler_descriptor = DescriptorSet::create_descriptor(scene_buffer.handle(), scene_buffer.light_sampler_size(), scene_buffer.get_light_sampler_offset());
    VkDescriptorBufferInfo queue_descriptor = DescriptorSet::create_descriptor(wavefront_queue.handle(), wavefront_queue.get_total_size());

    std::vector<VkDescriptorImageInfo> texture_descriptors;
    if (!scene_textures.get_textures().empty()) {

        if (scene_textures.get_textures().size() > MAX_TEXTURE_COUNT)
            throw std::runtime_error("Number of textures were more than the maximum number of " + MAX_TEXTURE_COUNT);

        texture_descriptors.resize(scene_textures.get_textures().size());

        for (size_t i = 0; i < scene_textures.get_textures().size(); i++)
            texture_descriptors[i] = DescriptorSet::create_descriptor(scene_textures.get_textures()[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture_sampler.handle());
    }

    VkDescriptorBufferInfo ubo_descriptors[IN_FLIGHT];
    for (size_t i = 0; i < IN_FLIGHT; i++) {
        ubo_descriptors[i] = DescriptorSet::create_descriptor(uniform_buffers.handle(), sizeof(PathTraceUniformData), i * sizeof(PathTraceUniformData));

        VkWriteDescriptorSetAccelerationStructureKHR descriptor_set_acceleration_structure_info;
        VkWriteDescriptorSet acceleration_structure_write = descriptor_sets[i].write_descriptor_set(acceleration_structure.get_top_level(), descriptor_set_acceleration_structure_info, 0);
        VkWriteDescriptorSet uniform_buffer_write = descriptor_sets[i].write_descriptor_set(&ubo_descriptors[i], 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        VkWriteDescriptorSet vertex_buffer_write = descriptor_sets[i].write_descriptor_set(&vertex_descriptor, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VkWriteDescriptorSet index_buffer_write = descriptor_sets[i].write_descriptor_set(&index_descriptor, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VkWriteDescriptorSet object_buffer_write = descriptor_sets[i].write_descriptor_set(&object_descriptor, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VkWriteDescriptorSet material_buffer_write = descriptor_sets[i].write_descriptor_set(&material_descriptor, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VkWriteDescriptorSet light_sampler_write = descriptor_sets[i].write_descriptor_set(&light_sampler_descriptor, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        VkWriteDescriptorSet queue_descriptor_write = descriptor_sets[i].write_descriptor_set(&queue_descriptor, 10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        std::vector<VkWriteDescriptorSet> write_descriptor_sets = {
            acceleration_structure_write,
            uniform_buffer_write,
            vertex_buffer_write,
            index_buffer_write,
            object_buffer_write,
            material_buffer_write,
            light_sampler_write,
            queue_descriptor_write
        };

        if (!scene_textures.get_textures().empty()) {
            VkWriteDescriptorSet texture_write = descriptor_sets[i].write_descriptor_set(
                texture_descriptors.data(), 8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, static_cast<uint32_t>(texture_descriptors.size()));
            write_descriptor_sets.push_back(texture_write);
        }

        DescriptorSet::update_write_descriptors(device, write_descriptor_sets.data(), static_cast<uint32_t>(write_descriptor_sets.size()));
    }
}

void PathTracer::set_scene(CommandPool& command_pool, const Scene& scene)
{
    scene_buffer.rebuild(command_pool, scene);
    scene_textures.rebuild(command_pool, scene);
    acceleration_structure.rebuild(command_pool, scene, scene_buffer);

    update_scene_uniform_data(scene);
    update_descriptor_sets();
}

void PathTracer::update_scene_uniform_data(const Scene& scene)
{
    uniform_data.environment_colour = scene.get_environment_colour();
    uniform_data.light_count = scene_buffer.get_light_count();
}

void PathTracer::set_camera(CommandPool& command_pool, const Camera& camera)
{
    Mat4 view = camera.view_matrix();
    Mat4 proj = camera.projection_matrix();

    uniform_data.inv_proj = glm::inverse(proj);
    uniform_data.inv_view = glm::inverse(view);
    uniform_data.near = camera.near;
    uniform_data.far = camera.far;
    uniform_data.focus_dist = camera.focus_dist;
    uniform_data.lens_radius = camera.lens_radius;

    uniform_data.sample_index = 0;
}

void PathTracer::set_samples(uint32_t samples)
{
    if (samples == 0)
        throw std::runtime_error("Sample count was zero.");

    uniform_data.samples = samples;
}

void PathTracer::set_max_bounces(uint32_t max_bounces)
{
    uniform_data.sample_index = 0;
    uniform_data.max_bounces = max_bounces;
}

void PathTracer::write_command_buffer(VkCommandBuffer command_buffer, size_t flight_index)
{
    assert(flight_index < IN_FLIGHT);
    const VkDeviceSize shader_group_buffer_address = device.get_buffer_address(shader_group_buffer);

    uint32_t aligned_handle_size, base_alignment;
    get_shader_group_alignments(device, aligned_handle_size, base_alignment);

    VkDeviceAddress base = shader_group_buffer_address;
    VkStridedDeviceAddressRegionKHR ray_gen_sbt{};
    ray_gen_sbt.deviceAddress = base;
    ray_gen_sbt.stride = aligned_handle_size;
    ray_gen_sbt.size = aligned_handle_size * SHADER_BINDING_TABLE_ENTRY_COUNTS[0];
    base += round_up_to<VkDeviceSize>(ray_gen_sbt.size, base_alignment);

    VkStridedDeviceAddressRegionKHR miss_sbt{};
    miss_sbt.deviceAddress = base;
    miss_sbt.stride = aligned_handle_size;
    miss_sbt.size = aligned_handle_size * SHADER_BINDING_TABLE_ENTRY_COUNTS[1];
    base += round_up_to<VkDeviceSize>(miss_sbt.size, base_alignment);

    VkStridedDeviceAddressRegionKHR hit_sbt{};
    hit_sbt.deviceAddress = base;
    hit_sbt.stride = aligned_handle_size;
    hit_sbt.size = aligned_handle_size * SHADER_BINDING_TABLE_ENTRY_COUNTS[2];
    base += round_up_to<VkDeviceSize>(hit_sbt.size, base_alignment);

    VkStridedDeviceAddressRegionKHR callable_sbt{};
    callable_sbt.deviceAddress = base;
    callable_sbt.stride = aligned_handle_size;
    callable_sbt.size = aligned_handle_size * SHADER_BINDING_TABLE_ENTRY_COUNTS[3];

    resetter.write_command_buffer(command_buffer, flight_index);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_sets[flight_index].handle(), 0, nullptr);

    if (USE_WAVEFRONT) {
        size_t max_passes = (size_t)std::ceil((float)(uniform_data.width * uniform_data.height) / wavefront_queue.get_queue_max_count());
        size_t dispatches = (uniform_data.max_bounces + 1) * max_passes;
        for (size_t d = 0; d < dispatches; d++) {
            memory_barrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            queue_swapper.write_command_buffer(command_buffer, flight_index);
            memory_barrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            vkCmdTraceRaysKHR(
                command_buffer,
                &ray_gen_sbt,
                &miss_sbt,
                &hit_sbt,
                &callable_sbt,
                wavefront_queue.get_queue_max_count(),
                1,
                1);
        }
    } else {
        vkCmdTraceRaysKHR(
            command_buffer,
            &ray_gen_sbt,
            &miss_sbt,
            &hit_sbt,
            &callable_sbt,
            uniform_data.width,
            uniform_data.height,
            1);
    }
}

void PathTracer::set_accumulation_image(CommandPool& command_pool, StorageImage& image)
{
    uniform_data.width = image.get_extent().width;
    uniform_data.height = image.get_extent().height;
    uniform_data.sample_index = 0;
    resetter.set_group_counts(resetter.compute_group_counts_2d(image.get_extent().width, image.get_extent().height, PTRESET_GROUP_SIZE));

    VkDescriptorImageInfo image_descriptor = DescriptorSet::create_descriptor(image.get_view(), VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet write_descriptor_sets[IN_FLIGHT];
    for (size_t i = 0; i < IN_FLIGHT; i++)
        write_descriptor_sets[i] = descriptor_sets[i].write_descriptor_set(&image_descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    DescriptorSet::update_write_descriptors(device, write_descriptor_sets, IN_FLIGHT);
}

void PathTracer::update_uniforms(size_t flight_index)
{
    memcpy((PathTraceUniformData*)uniform_buffers.get_map() + flight_index, &uniform_data, sizeof(PathTraceUniformData));

    uniform_data.sample_index += uniform_data.samples;
}