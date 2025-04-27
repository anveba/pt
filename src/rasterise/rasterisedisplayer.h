#ifndef RASTERISE_RASTERISEDISPLAYER_H_INCLUDED
#define RASTERISE_RASTERISEDISPLAYER_H_INCLUDED

#include "display/display.h"
#include "display/displayable.h"
#include "graphics/fence.h"
#include "rasterise/rasterise.h"
#include "semaphore.h"

class RasteriseDisplayer : public IDisplayable
{
  public:
    RasteriseDisplayer(Display& display,
                       DescriptorPool& descriptor_pool,
                       CommandPool& command_pool,
                       const Scene& scene,
                       VkExtent2D extent,
                       VkFormat image_format,
                       VkFormat depth_format);
    ~RasteriseDisplayer();

    virtual void set_extent(uint32_t width, uint32_t height) override;

    virtual void set_scene(const Scene& scene) override;
    virtual void set_camera(const Camera& camera) override;

    virtual void wait_idle() override;
    virtual void begin_render() override;
    virtual void end_render() override;

    virtual void get_debug_info(RenderDebugInfo& info) const override;
    virtual void set_settings(const UiControlPanel& control_panel) override;
    virtual RenderType render_type() const override { return RENDER_TYPE_RASTERISE; };
    virtual size_t max_in_flight() const override { return 1; };

  private:
    virtual VkRenderPass get_render_pass() override;
    virtual VkCommandBuffer get_command_buffer() override;

    VkImage depth_image;
    VkImageView depth_image_view;
    VkDeviceMemory depth_image_memory;

    std::vector<VkFramebuffer> framebuffers;

    // TODO: in flight
    Semaphore image_semaphore;
    Semaphore render_semaphore;
    Fence render_fence;

    Display& display;
    CommandPool& command_pool;
    Rasteriser rasteriser;
    VkCommandBuffer command_buffer;

    bool in_render;

    void create_framebuffers();
    void destroy_framebuffers();
    void create_depth_image();
    void destroy_depth_image();

    NO_COPY(RasteriseDisplayer);
};

#endif