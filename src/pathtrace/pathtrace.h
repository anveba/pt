#ifndef PATHTRACE_PATHTRACE_H_INCLUDED
#define PATHTRACE_PATHTRACE_H_INCLUDED

#include "accstruct.h"
#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"
#include "rng.h"
#include "scene/scene.h"
#include "scene/scenebuffer.h"

class PathTracer
{
  public:
    PathTracer(Device& device,
               DescriptorPool& descriptor_pool,
               CommandPool& command_pool,
               const Scene& scene,
               VkExtent2D extent);
    ~PathTracer();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

    uint32_t get_samples_per_render() { return uniform_map->samples; }
    uint32_t get_max_bounces() { return uniform_map->max_bounces; }

    void set_scene(CommandPool& command_pool, const Scene& scene);
    void set_camera(CommandPool& command_pool, const Camera& camera);
    void set_samples(uint32_t samples);
    void set_max_bounces(uint32_t max_bounces);
    void set_extent(CommandPool& command_pool, uint32_t width, uint32_t height);

  private:
    void update_uniforms();

    Device& device;
    VkExtent2D extent;

    SceneBuffer<PathTraceInstanceData> scene_buffer;
    AccelerationStructure acceleration_structure;

    struct UniformBufferObject
    {
        alignas(16) Mat4 inv_view;
        alignas(16) Mat4 inv_proj;
        alignas(4) float near;
        alignas(4) float far;
        alignas(4) float old_samples_mult;
        alignas(4) float new_samples_mult;
        alignas(16) Uint4 seed;
        alignas(4) uint32_t samples;
        alignas(4) uint32_t max_bounces;
    };
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    UniformBufferObject* uniform_map;

    VkImage dest_image;
    VkImageView dest_image_view;
    VkDeviceMemory dest_image_memory;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    DescriptorSetLayout descriptor_set_layout;
    DescriptorSet descriptor_set;

    VkBuffer shader_group_buffer;
    VkDeviceMemory shader_group_buffer_memory;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    bool in_render;

    uint32_t samples_taken;
    Xshiro128 rng;

    void write_command_buffer(VkCommandBuffer command_buffer);

    void create_dest_image(CommandPool& command_pool);
    void create_shader_binding_tables();
    void create_pipeline();
    void update_descriptor_sets();

    void create_blas(CommandPool& command_pool);
    void create_tlas(CommandPool& command_pool);
    void create_acceleration_structure(VkAccelerationStructureKHR* out,
                                       CommandPool& command_pool,
                                       VkBuffer acc_struct_buffer,
                                       const VkAccelerationStructureBuildSizesInfoKHR* size_infos,
                                       const VkAccelerationStructureGeometryKHR* geometries,
                                       const VkAccelerationStructureBuildRangeInfoKHR* const* range_info_ptrs,
                                       VkDeviceSize max_scratch_size,
                                       const size_t count,
                                       VkAccelerationStructureTypeKHR type);

    void destroy_dest_image();

    friend class PathTraceDisplayer;

    NO_COPY(PathTracer);
};

#endif