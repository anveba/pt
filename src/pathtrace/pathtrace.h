#ifndef PATHTRACE_PATHTRACE_H_INCLUDED
#define PATHTRACE_PATHTRACE_H_INCLUDED

#include "graphics/dispatch.h"
#include "graphics/shader.h"
#include "rng.h"
#include "scene/scene.h"

class PathTracer
{
  public:
    PathTracer(Dispatcher& dispatcher,
               const Scene& scene,
               VkExtent2D extent);
    ~PathTracer();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

    uint32_t get_samples_per_render() { return uniform_map->samples; }
    uint32_t get_max_bounces() { return uniform_map->max_bounces; }

  private:
    void set_scene(Dispatcher& dispatcher, const Scene& scene);
    void set_camera(Dispatcher& dispatcher, const Camera& camera);
    void set_samples(uint32_t samples);
    void set_max_bounces(uint32_t max_bounces);

    void wait_for_render();
    void begin_render();
    VkSemaphore end_render(VkSemaphore* wait_for, uint32_t semaphore_count);

    void set_extent(uint32_t width, uint32_t height);

    void copy_result(VkImage image);

    Device& device;
    Dispatcher& dispatcher;
    VkExtent2D extent;

    // TODO: combine buffers
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    std::vector<uint32_t> vertex_end_indices;
    VkDeviceAddress vertex_buffer_address;

    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    std::vector<uint32_t> index_end_indices;
    VkDeviceAddress index_buffer_address;

    VkBuffer object_buffer;
    VkDeviceMemory object_buffer_memory;
    VkDeviceAddress object_buffer_address;

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

    VkBuffer blas_buffer;
    VkDeviceMemory blas_memory;
    std::vector<VkAccelerationStructureKHR> blas;

    VkBuffer tlas_buffer;
    VkDeviceMemory tlas_memory;
    VkAccelerationStructureKHR tlas;

    VkImage dest_image;
    VkImageView dest_image_view;
    VkDeviceMemory dest_image_memory;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;

    VkBuffer shader_group_buffer;
    VkDeviceMemory shader_group_buffer_memory;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    VkCommandBuffer command_buffer;

    VkSemaphore render_semaphore;
    VkFence render_fence;

    const Scene* scene;
    bool in_render;

    uint32_t samples_taken;
    Xshiro128 rng;

    void create_dest_image(Dispatcher& dispatcher);
    void write_command_buffer();

    void create_shader_binding_tables();
    void create_descriptor_set_layout();
    void create_pipeline();
    void create_descriptor_sets(Dispatcher& dispatcher);
    void create_command_buffer(Dispatcher& dispatcher);
    void create_sync_objects();

    void create_blas(Dispatcher& dispatcher);
    void create_tlas(Dispatcher& dispatcher);
    void create_acceleration_structure(VkAccelerationStructureKHR* out,
                                       Dispatcher& dispatcher,
                                       VkBuffer acc_struct_buffer,
                                       const VkAccelerationStructureBuildSizesInfoKHR* size_infos,
                                       const VkAccelerationStructureGeometryKHR* geometries,
                                       const VkAccelerationStructureBuildRangeInfoKHR* const* range_info_ptrs,
                                       VkDeviceSize max_scratch_size,
                                       const size_t count,
                                       VkAccelerationStructureTypeKHR type);

    void free_scene_buffers();
    void destroy_dest_image();

    friend class PathTraceDisplayer;

    NO_COPY(PathTracer);
};

#endif