#ifndef RASTERISER_H_INCLUDED
#define RASTERISER_H_INCLUDED

#include "renderer.h"
#include "scene.h"
#include "shader.h"

class Rasteriser : public IRenderer
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

    virtual void set_scene(Dispatcher& dispatcher, const Scene& scene) override;
    virtual void begin_render(IRenderTarget* render_target) override;
    virtual VkSemaphore end_render() override;

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Device* const device;
    const VkExtent2D extent;

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
    VkDescriptorSet descriptor_set;

    VkCommandBuffer command_buffer;

    VkSemaphore render_semaphore;
    VkFence render_fence;
    VkSemaphore image_semaphore;

    VkImage depth_image;
    VkImageView depth_image_view;
    VkDeviceMemory depth_image_memory;
    VkFormat depth_format;

    const Scene* scene;
    bool in_render;

    virtual VkRenderPass get_render_pass() override { return render_pass; };
    virtual std::vector<VkImageView> get_extra_attachments() override { return { depth_image_view }; }

    void create_render_pass(VkFormat image_format, VkFormat depth_format);
    void create_descriptor_set_layout();
    void create_pipeline(const Shader& vs, const Shader& ps);
    void create_descriptor_set(Dispatcher& dispatcher);
    void create_command_buffer(Dispatcher& dispatcher);
    void create_depth_image(VkFormat depth_format, VkExtent2D extent);
    void create_sync_objects();
    void write_command_buffer(VkFramebuffer framebuffer);

    void update_uniforms();

    friend class UserInterface;

    Rasteriser(Rasteriser const&) = delete;
    void operator=(Rasteriser const& x) = delete;
};

#endif