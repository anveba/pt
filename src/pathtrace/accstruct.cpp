#include "accstruct.h"

static void create_acceleration_structure(
    VkAccelerationStructureKHR* out,
    Device& device,
    CommandPool& command_pool,
    VkBuffer acc_struct_buffer,
    const VkAccelerationStructureBuildSizesInfoKHR* size_infos,
    const VkAccelerationStructureGeometryKHR* geometries,
    const VkAccelerationStructureBuildRangeInfoKHR* const* range_info_ptrs,
    VkDeviceSize total_scratch_size,
    const size_t count,
    VkAccelerationStructureTypeKHR type)
{
    VkBuffer scratch_buffer;
    VkDeviceMemory scratch_buffer_memory;
    device.create_buffer(scratch_buffer,
                         scratch_buffer_memory,
                         total_scratch_size,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
    VkDeviceAddress scratch_buffer_address = device.get_buffer_address(scratch_buffer);

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_geometry_infos;
    build_geometry_infos.resize(count);
    VkDeviceSize acc_struct_offset = 0, scratch_buffer_offset = 0;

    for (size_t i = 0; i < count; i++) {

        VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
        acceleration_structure_create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        acceleration_structure_create_info.buffer = acc_struct_buffer;
        acceleration_structure_create_info.size = size_infos[i].accelerationStructureSize;
        acceleration_structure_create_info.type = type;
        acceleration_structure_create_info.offset = acc_struct_offset;
        vkCreateAccelerationStructureKHR(device.logical_handle(), &acceleration_structure_create_info, nullptr, &out[i]);

        VkAccelerationStructureBuildGeometryInfoKHR& build_geometry = build_geometry_infos[i];
        build_geometry = {};
        build_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_geometry.type = type;
        build_geometry.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build_geometry.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_geometry.dstAccelerationStructure = out[i];
        build_geometry.geometryCount = 1;
        build_geometry.pGeometries = &geometries[i];
        build_geometry.scratchData.deviceAddress = scratch_buffer_address + scratch_buffer_offset;

        acc_struct_offset += round_up_to<VkDeviceSize>(size_infos[i].accelerationStructureSize, 256);
        scratch_buffer_offset += size_infos[i].buildScratchSize;
    }

    VkCommandBuffer command_buffer = command_pool.begin_one_time_use_command_buffer();

    vkCmdBuildAccelerationStructuresKHR(
        command_buffer,
        count,
        build_geometry_infos.data(),
        range_info_ptrs);

    command_pool.end_one_time_use_command_buffer(command_buffer, device.get_graphics_queue());

    vkFreeMemory(device.logical_handle(), scratch_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), scratch_buffer, nullptr);
}

void AccelerationStructure::create_blas(CommandPool& command_pool)
{
    assert(scene_buffer != nullptr);

    const size_t blas_count = scene_buffer->get_scene().get_object_variants().size();

    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> range_infos;
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> size_infos;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> range_info_ptrs;
    geometries.resize(blas_count);
    range_infos.resize(blas_count);
    size_infos.resize(blas_count);
    range_info_ptrs.resize(blas_count);

    VkDeviceSize total_blas_size = 0;
    VkDeviceSize total_scratch_size = 0;

    VkDeviceAddress scene_buffer_address = device.get_buffer_address(scene_buffer->handle());

    for (size_t i = 0; i < blas_count; i++) {

        const ObjectVariant& object = scene_buffer->get_scene().get_object_variants()[i];
        const BufferIndices& start_indices = scene_buffer->get_start_indices(i);

        VkDeviceOrHostAddressConstKHR vertex_address_const{
            .deviceAddress = scene_buffer_address + scene_buffer->get_vertex_offset() + start_indices.vertex * sizeof(Vertex)
        };
        VkDeviceOrHostAddressConstKHR index_address_const{
            .deviceAddress = scene_buffer_address + scene_buffer->get_index_offset() + start_indices.index * sizeof(uint32_t)
        };

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
        total_scratch_size += size.buildScratchSize;
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
        device,
        command_pool,
        blas_buffer,
        size_infos.data(),
        geometries.data(),
        range_info_ptrs.data(),
        total_scratch_size,
        blas_count,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
}

static VkTransformMatrixKHR to_vk_transform_matrix(const Mat4& mat)
{
    VkTransformMatrixKHR vkTransformMatrix = {};

    vkTransformMatrix.matrix[0][0] = mat[0][0];
    vkTransformMatrix.matrix[0][1] = mat[0][1];
    vkTransformMatrix.matrix[0][2] = mat[0][2];
    vkTransformMatrix.matrix[0][3] = mat[0][3];
    vkTransformMatrix.matrix[1][0] = mat[1][0];
    vkTransformMatrix.matrix[1][1] = mat[1][1];
    vkTransformMatrix.matrix[1][2] = mat[1][2];
    vkTransformMatrix.matrix[1][3] = mat[1][3];
    vkTransformMatrix.matrix[2][0] = mat[2][0];
    vkTransformMatrix.matrix[2][1] = mat[2][1];
    vkTransformMatrix.matrix[2][2] = mat[2][2];
    vkTransformMatrix.matrix[2][3] = mat[2][3];

    return vkTransformMatrix;
}

void AccelerationStructure::create_tlas(CommandPool& command_pool)
{
    assert(scene_buffer != nullptr);
    assert(blas.size() == scene_buffer->get_scene().get_object_variants().size());

    size_t total_instance_count = 0;
    for (const ObjectVariant& object_variant : scene_buffer->get_scene().get_object_variants())
        total_instance_count += object_variant.instances.size();

    VkAccelerationStructureInstanceKHR* instances = new VkAccelerationStructureInstanceKHR[total_instance_count];

    uint32_t instance_index = 0;

    for (size_t i = 0; i < scene_buffer->get_scene().get_object_variants().size(); i++) {
        VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
        acceleration_device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        acceleration_device_address_info.accelerationStructure = blas[i];
        VkDeviceAddress blas_address = vkGetAccelerationStructureDeviceAddressKHR(device.logical_handle(), &acceleration_device_address_info);

        const ObjectVariant& object_variant = scene_buffer->get_scene().get_object_variants()[i];

        for (size_t j = 0; j < object_variant.instances.size(); j++) {
            VkAccelerationStructureInstanceKHR& instance = instances[instance_index];
            instance = {};
            instance.transform = to_vk_transform_matrix(object_variant.instances[j].transform.matrix);
            instance.instanceCustomIndex = instance_index;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = blas_address;
            instance_index++;
        }
    }

    VkBuffer instance_buffer;
    VkDeviceMemory instance_buffer_memory;

    device.create_buffer(instance_buffer,
                         instance_buffer_memory,
                         total_instance_count * sizeof(VkAccelerationStructureInstanceKHR),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    command_pool.transfer_to_buffer(instance_buffer,
                                    instances,
                                    total_instance_count * sizeof(VkAccelerationStructureInstanceKHR));
    delete[] instances;
    VkDeviceOrHostAddressConstKHR instance_buffer_address = { .deviceAddress = device.get_buffer_address(instance_buffer) };

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
                                  device,
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

AccelerationStructure::AccelerationStructure(Device& device, CommandPool& command_pool, const SceneBuffer<PathTraceInstanceData>& scene_buffer)
    : device(device)
    , scene_buffer(&scene_buffer)
{
    create_blas(command_pool);
    create_tlas(command_pool);
}

AccelerationStructure::~AccelerationStructure()
{
    free();
}

void AccelerationStructure::free()
{
    vkDestroyBuffer(device.logical_handle(), tlas_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), tlas_memory, nullptr);
    vkDestroyBuffer(device.logical_handle(), blas_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), blas_memory, nullptr);
    vkDestroyAccelerationStructureKHR(device.logical_handle(), tlas, nullptr);
    for (auto b : blas)
        vkDestroyAccelerationStructureKHR(device.logical_handle(), b, nullptr);
}

void AccelerationStructure::rebuild(CommandPool& command_pool, const SceneBuffer<PathTraceInstanceData>& scene_buffer)
{
    free();
    this->scene_buffer = &scene_buffer;
    create_blas(command_pool);
    create_tlas(command_pool);
}
