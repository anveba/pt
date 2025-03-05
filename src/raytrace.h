#ifndef RAYTRACE_H_INCLUDED
#define RAYTRACE_H_INCLUDED

#include "dispatch.h"
#include "scene.h"
#include "shader.h"

class RayTracer
{
  public:
    RayTracer(Device& device,
              Dispatcher& dispatcher,
              const Scene& scene,
              Shader& ray_gen,
              Shader& ray_miss,
              Shader& ray_closest_hit,
              VkExtent2D extent);
    ~RayTracer();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    void set_scene(Dispatcher& dispatcher, const Scene& scene);
    void set_camera(Dispatcher& dispatcher, const Camera& camera);

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

    struct UniformBufferObject
    {
        alignas(16) Mat4 inv_view;
        alignas(16) Mat4 inv_proj;
        alignas(4) float near;
        alignas(4) float far;
        alignas(4) float old_samples_mult;
        alignas(4) float new_samples_mult;
        alignas(4) uint32_t seed;
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
    uint32_t current_sample;

    void create_dest_image(Dispatcher& dispatcher);
    void write_command_buffer();

    void create_shader_binding_tables();
    void create_descriptor_set_layout();
    void create_pipeline(Shader& ray_gen, Shader& ray_miss, Shader& ray_hit);
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

    friend class RayTraceDisplayer;

    RayTracer(RayTracer const&) = delete;
    void operator=(RayTracer const& x) = delete;
};

#endif