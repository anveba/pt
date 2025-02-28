#ifndef RASTERISER_H_INCLUDED
#define RASTERISER_H_INCLUDED

#include "renderer.h"
#include "shader.h"

struct UniformBufferObject;

class Rasteriser : public IRenderer
    , public IDisplayable
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
    virtual void set_camera(Dispatcher& dispatcher, const Camera& camera) override;

    virtual void begin_render(IRenderTarget* render_target) override;
    virtual VkSemaphore end_render() override;

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Device* const device;
    const VkExtent2D extent;

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

    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_buffer_memory;
    UniformBufferObject* uniform_buffer_map;

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

    void free_scene_buffers();

    friend class UserInterface;

    Rasteriser(Rasteriser const&) = delete;
    void operator=(Rasteriser const& x) = delete;
};

#endif