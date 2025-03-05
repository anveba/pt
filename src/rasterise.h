#ifndef RASTERISE_H_INCLUDED
#define RASTERISE_H_INCLUDED

#include "dispatch.h"
#include "scene.h"
#include "shader.h"

class Rasteriser
{
  public:
    Rasteriser(Device& device,
               Dispatcher& dispatcher,
               const Shader& vs,
               const Shader& ps,
               VkExtent2D extent,
               VkFormat image_format,
               VkFormat depth_format);
    ~Rasteriser();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Device& device;
    VkExtent2D extent;

    VkRenderPass render_pass;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    // TODO: combine buffers
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    std::vector<uint32_t> vertex_end_indices;

    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    std::vector<uint32_t> index_end_indices;

    VkBuffer instance_buffer;
    VkDeviceMemory instance_buffer_memory;
    std::vector<uint32_t> instance_end_indices;

    struct UniformBufferObject
    {
        alignas(16) Mat4 mvp;
        alignas(16) Vec3 view_pos;
        alignas(16) Vec3 inv_light_dir_norm;
        alignas(16) Mat3x4 normal;
    };
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    UniformBufferObject* uniform_buffer_map;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;

    VkCommandBuffer command_buffer;

    VkSemaphore render_semaphore;
    VkFence render_fence;

    const VkFormat depth_format;

    const Scene* scene;
    bool in_render;

    void set_scene(Dispatcher& dispatcher, const Scene& scene);
    void set_camera(Dispatcher& dispatcher, const Camera& camera);

    void wait_for_render();
    void begin_render(VkFramebuffer framebuffer);
    VkSemaphore end_render(VkSemaphore* wait_for, uint32_t semaphore_count);

    void set_extent(uint32_t width, uint32_t height);

    void create_render_pass(VkFormat image_format, VkFormat depth_format);
    void create_descriptor_set_layout();
    void create_pipeline(const Shader& vs, const Shader& ps);
    void create_descriptor_set(Dispatcher& dispatcher);
    void create_command_buffer(Dispatcher& dispatcher);
    void create_sync_objects();
    void write_command_buffer(VkFramebuffer framebuffer);

    void free_scene_buffers();

    friend class RasteriseDisplayer;

    Rasteriser(Rasteriser const&) = delete;
    void operator=(Rasteriser const& x) = delete;
};

#endif