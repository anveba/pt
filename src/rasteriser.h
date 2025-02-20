#ifndef RASTERISER_H_INCLUDED
#define RASTERISER_H_INCLUDED

#include "renderer.h"
#include "scene.h"
#include "shader.h"

class Rasteriser : IRenderer
{
  public:
    Rasteriser(const Shader& vs, const Shader& ps, Dispatcher& dispatcher);
    ~Rasteriser();

    virtual void set_scene(const Scene& scene) override;
    virtual void new_frame() override;
    virtual void end_frame() override;

  private:
    Dispatcher* dispatcher; // TODO, get rid of raw pointer

    VkRenderPass render_pass;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    void* uniform_buffer_map;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

    std::vector<VkFramebuffer> framebuffers;

    VkSemaphore image_semaphore;
    VkSemaphore render_semaphore;
    VkFence render_fence;

    // TODO move?
    VkImage depth_image;
    VkImageView depth_image_view;
    VkDeviceMemory depth_image_memory;

    uint32_t current_image_index;

    const Scene* scene;

    void create_render_pass();
    void create_descriptor_set_layout();
    void create_pipeline(const Shader& vs, const Shader& ps);
    void create_framebuffers();
    void create_descriptor_pool();
    void create_command_pool();
    void create_depth_image();
    void create_descriptor_set();
    void create_command_buffer();
    void create_sync_objects();
    void write_command_buffer(size_t image_idx);

    // TODO: move elsewhere (to dispatcher?)
    void create_buffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_flags);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void transfer_to_buffer(VkBuffer& buffer, VkDeviceMemory& memory, const void* src_data, size_t size, VkBufferUsageFlags flags);
    void create_image(VkImage& image,
                      VkDeviceMemory& memory,
                      uint32_t width,
                      uint32_t height,
                      VkFormat format,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VkMemoryPropertyFlags properties);
    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask);
    VkFormat find_image_format(const std::vector<VkFormat>& desirable,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
    uint32_t find_suitable_memory_type(int32_t typeFilter, VkMemoryPropertyFlags desired_flags);

    void update_uniforms();
    VkFormat depth_format();

    friend class UserInterface;

    Rasteriser(Rasteriser const&) = delete;
    void operator=(Rasteriser const& x) = delete;
};

#endif