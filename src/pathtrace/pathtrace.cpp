#include "pathtrace.h"

#include <cassert>

struct ObjectData
{
    uint32_t vertex_index;
    uint32_t index_index;
    uint32_t material_index;
};

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

PathTracer::PathTracer(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    VkExtent2D extent)
    : device(device)
    , extent(extent)
    , descriptor_set_layout(device, get_descriptor_set_layout_bindings())
    , descriptor_set(descriptor_pool, descriptor_set_layout)
    , scene(nullptr)
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
    assert(scene != nullptr);

    free_scene_buffers();

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

    command_pool.end_one_time_use_command_buffer(command_buffer, device.graphics_queue);
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

    if (vkCreatePipelineLayout(device.logical, &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout.");

    Shader ray_gen(device, "bin/shaders/pathtrace/pathtrace.rg.spv");
    Shader ray_miss(device, "bin/shaders/pathtrace/pathtrace.ms.spv");
    Shader ray_closest_hit(device, "bin/shaders/pathtrace/pathtrace.ch.spv");

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
    closest_hit_stage.module = ray_closest_hit.shader_module;
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
    raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = device.physical_device_info.ray_tracing_properties.maxRayRecursionDepth; // TODO
    raytracing_pipeline_create_info.layout = pipeline_layout;

    if (vkCreateRayTracingPipelinesKHR(device.logical_handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ray tracing pipeline.");
}

void PathTracer::update_descriptor_sets()
{
    VkDescriptorImageInfo dest_image_descriptor = DescriptorSet::create_descriptor(dest_image_view, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorBufferInfo ubo_descriptor = DescriptorSet::create_descriptor(uniform_buffer, sizeof(UniformBufferObject));
    VkDescriptorBufferInfo vertex_descriptor = DescriptorSet::create_descriptor(vertex_buffer, vertex_end_indices.back() * sizeof(Vertex));
    VkDescriptorBufferInfo index_descriptor = DescriptorSet::create_descriptor(index_buffer, index_end_indices.back() * sizeof(uint32_t));
    VkDescriptorBufferInfo object_descriptor = DescriptorSet::create_descriptor(object_buffer, scene->get_object_variants().size() * sizeof(ObjectData));

    VkWriteDescriptorSetAccelerationStructureKHR descriptor_set_acceleration_structure_info;
    VkWriteDescriptorSet acceleration_structure_write = descriptor_set.write_descriptor_set(tlas, descriptor_set_acceleration_structure_info, 0);
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

void PathTracer::free_scene_buffers()
{
    assert(scene != nullptr);

    vkDestroyBuffer(device.logical_handle(), vertex_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), index_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), index_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), object_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), object_buffer_memory, nullptr);

    vkDestroyBuffer(device.logical_handle(), tlas_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), tlas_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), blas_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), blas_memory, nullptr);
    vkDestroyAccelerationStructureKHR(device.logical_handle(), tlas, nullptr);
    for (auto b : blas)
        vkDestroyAccelerationStructureKHR(device.logical_handle(), b, nullptr);
}

static VkDeviceAddress get_buffer_address(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = buffer;

    return vkGetBufferDeviceAddress(device, &address_info);
}

void PathTracer::set_scene(CommandPool& command_pool, const Scene& scene)
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

    command_pool.transfer_to_buffer(vertex_buffer,
                                    all_vertices,
                                    vertex_count * sizeof(Vertex));
    vertex_buffer_address = get_buffer_address(device.logical_handle(), vertex_buffer);

    device.create_buffer(index_buffer,
                         index_buffer_memory,
                         tri_count * 3 * sizeof(uint32_t),
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | common_buffer_usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    command_pool.transfer_to_buffer(index_buffer,
                                    all_indices,
                                    tri_count * 3 * sizeof(uint32_t));
    index_buffer_address = get_buffer_address(device.logical_handle(), index_buffer);

    device.create_buffer(object_buffer,
                         object_buffer_memory,
                         object_variants.size() * sizeof(ObjectData),
                         common_buffer_usage,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    command_pool.transfer_to_buffer(object_buffer,
                                    all_object_data,
                                    object_variants.size() * sizeof(ObjectData));

    delete[] all_vertices;
    delete[] all_indices;
    delete[] all_object_data;

    create_blas(command_pool);
    create_tlas(command_pool);

    update_descriptor_sets();
}

void PathTracer::set_camera(CommandPool& command_pool, const Camera& camera)
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

void PathTracer::create_blas(CommandPool& command_pool)
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
            device.logical_handle(),
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
        command_pool,
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

void PathTracer::create_tlas(CommandPool& command_pool)
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

    command_pool.transfer_to_buffer(instance_buffer,
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
        device.logical_handle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_geometry_info, &primitive_count, &build_sizes_info);

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
                                  command_pool,
                                  tlas_buffer,
                                  &build_sizes_info,
                                  &geometry,
                                  &build_range_info_ptr,
                                  build_sizes_info.buildScratchSize,
                                  1,
                                  VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

    vkFreeMemory(device.logical_handle(), instance_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), instance_buffer, nullptr);
}

void PathTracer::create_acceleration_structure(
    VkAccelerationStructureKHR* out,
    CommandPool& command_pool,
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
    VkDeviceAddress scratch_buffer_address = get_buffer_address(device.logical_handle(), scratch_buffer);

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

    VkCommandBuffer command_buffer = command_pool.begin_one_time_use_command_buffer();

    vkCmdBuildAccelerationStructuresKHR(
        command_buffer,
        count,
        build_geometry_infos.data(),
        range_info_ptrs);

    command_pool.end_one_time_use_command_buffer(command_buffer, device.graphics_queue);

    vkFreeMemory(device.logical_handle(), scratch_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), scratch_buffer, nullptr);
}

void PathTracer::write_command_buffer(VkCommandBuffer command_buffer)
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
