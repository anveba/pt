#ifndef RASTERISE_RASTERISE_H_INCLUDED
#define RASTERISE_RASTERISE_H_INCLUDED

#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"
#include "graphics/ubo.h"
#include "scene/scene.h"
#include "scene/scenebuffer.h"

struct RasteriseUniformData
{
    alignas(16) Mat4 mvp;
    alignas(16) Vec3 view_pos;
    alignas(16) Vec3 inv_light_dir_norm;
    alignas(16) Mat3x4 normal;
};

class Rasteriser
{
  public:
    Rasteriser(Device& device,
               DescriptorPool& descriptor_pool,
               CommandPool& command_pool,
               const Scene& scene,
               VkExtent2D extent,
               VkFormat image_format,
               VkFormat depth_format);
    ~Rasteriser();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Device& device;
    VkExtent2D extent;

    SceneBuffer<RasteriseInstanceData> scene_buffer;

    RasteriseUniformData uniform_data;
    UniformBuffer uniform_buffer;

    VkRenderPass render_pass;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    DescriptorSetLayout descriptor_set_layout;
    DescriptorSet descriptor_set;

    const VkFormat depth_format;

    void set_scene(CommandPool& command_pool, const Scene& scene);
    void set_camera(CommandPool& command_pool, const Camera& camera);

    void set_extent(uint32_t width, uint32_t height);

    void create_render_pass(VkFormat image_format, VkFormat depth_format);
    void create_pipeline();
    void update_descriptor_set();
    void update_uniforms();
    void write_command_buffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer);

    friend class RasteriseDisplayer;

    NO_COPY(Rasteriser);
};

#endif