#ifndef PATHTRACE_PATHTRACE_H_INCLUDED
#define PATHTRACE_PATHTRACE_H_INCLUDED

#include "accstruct.h"
#include "compute/compute.h"
#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/sampler.h"
#include "graphics/shader.h"
#include "graphics/sharedmem.h"
#include "graphics/storageimage.h"
#include "initablearray.h"
#include "rng.h"
#include "scene/scene.h"
#include "scene/scenebuffer.h"
#include "scene/scenetex.h"
#include "wavequeue.h"

struct PathTraceUniformData
{
    alignas(16) Mat4 inv_view;
    alignas(16) Mat4 inv_proj;
    alignas(4) float near;
    alignas(4) float far;
    alignas(4) float focus_dist;
    alignas(4) float lens_radius;

    alignas(4) uint32_t sample_index;
    alignas(4) uint32_t samples;
    alignas(4) uint32_t max_bounces;

    alignas(4) uint32_t width;
    alignas(4) uint32_t height;
    alignas(4) uint32_t queue_capacity;

    alignas(4) uint32_t light_count;
    alignas(16) Vec3 environment_colour;
};

class PathTracer
{
  public:
    PathTracer(Device& device,
               DescriptorPool& descriptor_pool,
               CommandPool& command_pool,
               const Scene& scene,
               StorageImage& accumulation_image);
    ~PathTracer();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

    uint32_t get_samples_per_render() const { return uniform_data.samples; }
    uint32_t get_max_bounces() const { return uniform_data.max_bounces; }
    uint32_t get_sample_index() const { return uniform_data.sample_index; }

    void set_scene(CommandPool& command_pool, const Scene& scene);
    void set_camera(CommandPool& command_pool, const Camera& camera);
    void set_samples(uint32_t samples);
    void set_max_bounces(uint32_t max_bounces);
    void set_accumulation_image(CommandPool& command_pool, StorageImage& image);

    void update_uniforms(size_t flight_index);
    void write_command_buffer(VkCommandBuffer command_buffer, size_t flight_index);

    static constexpr size_t IN_FLIGHT = 3;

  private:
    static constexpr bool USE_WAVEFRONT = false;

    Device& device;

    SceneBuffer scene_buffer;
    SceneTextures scene_textures;
    AccelerationStructure acceleration_structure;
    WavefrontQueue wavefront_queue;

    PathTraceUniformData uniform_data;
    SharedMemory uniform_buffers;
    Sampler texture_sampler;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    DescriptorSetLayout descriptor_set_layout;
    InitableArray<DescriptorSet, IN_FLIGHT> descriptor_sets;

    Compute<IN_FLIGHT> resetter;
    Compute<IN_FLIGHT> queue_swapper;

    VkBuffer shader_group_buffer;
    VkDeviceMemory shader_group_buffer_memory;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    void create_shader_binding_tables();
    void create_pipeline();
    void update_descriptor_sets();
    void update_scene_uniform_data(const Scene& scene);

    NO_COPY(PathTracer);
};

#endif