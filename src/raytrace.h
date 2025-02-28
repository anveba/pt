#ifndef RAYTRACE_H_INCLUDED
#define RAYTRACE_H_INCLUDED

#include "renderer.h"
#include "shader.h"

class RayTracer : public IRenderer
{
  public:
    RayTracer(Device& device,
              Dispatcher& dispatcher,
              const Shader& vs,
              const Shader& ps,
              VkExtent2D extent,
              VkFormat image_format);
    ~RayTracer();

    virtual void set_scene(Dispatcher& dispatcher, const Scene& scene) override;
    virtual void set_camera(Dispatcher& dispatcher, const Camera& camera) override;

    virtual void begin_render(IRenderTarget* render_target) override;
    virtual VkSemaphore end_render() override;

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Device* const device;
    const VkExtent2D extent;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;

    VkCommandBuffer command_buffer;

    VkSemaphore render_semaphore;
    VkFence render_fence;
    VkSemaphore image_semaphore;

    const Scene* scene;
    bool in_render;

    void create_descriptor_set_layout();
    void create_pipeline();
    void create_descriptor_set(Dispatcher& dispatcher);
    void create_command_buffer(Dispatcher& dispatcher);
    void create_sync_objects();
    void write_command_buffer(VkFramebuffer framebuffer);

    void free_scene_buffers();

    RayTracer(RayTracer const&) = delete;
    void operator=(RayTracer const& x) = delete;
};

#endif