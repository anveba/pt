#ifndef PATHTRACE_PATHTRACE_H_INCLUDED
#define PATHTRACE_PATHTRACE_H_INCLUDED

#include "accstruct.h"
#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/sampler.h"
#include "graphics/shader.h"
#include "graphics/storageimage.h"
#include "graphics/ubo.h"
#include "rng.h"
#include "scene/scene.h"
#include "scene/scenebuffer.h"
#include "scene/scenetex.h"

struct PathTraceUniformData
{
    alignas(16) Vec3 environment_colour;
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

    uint32_t get_samples_per_render() { return uniform_data.samples; }
    uint32_t get_max_bounces() { return uniform_data.max_bounces; }
    uint32_t accumulated_samples() { return samples_taken; }
    const StorageImage& get_accumulation_image() { return accumulation_image; }

    void set_scene(CommandPool& command_pool, const Scene& scene);
    void set_camera(CommandPool& command_pool, const Camera& camera);
    void set_samples(uint32_t samples);
    void set_max_bounces(uint32_t max_bounces);
    void set_extent(CommandPool& command_pool, uint32_t width, uint32_t height);

    void update_uniforms();
    void write_command_buffer(VkCommandBuffer command_buffer);

  private:
    Device& device;

    SceneBuffer<PathTraceInstanceData> scene_buffer;
    SceneTextures scene_textures;
    AccelerationStructure acceleration_structure;

    PathTraceUniformData uniform_data;
    UniformBuffer uniform_buffer;
    StorageImage accumulation_image;
    Sampler texture_sampler;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    DescriptorSetLayout descriptor_set_layout;
    DescriptorSet descriptor_set;

    VkBuffer shader_group_buffer;
    VkDeviceMemory shader_group_buffer_memory;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    uint32_t samples_taken;
    Xshiro128 rng;

    void create_shader_binding_tables();
    void create_pipeline();
    void update_descriptor_sets();

    NO_COPY(PathTracer);
};

#endif