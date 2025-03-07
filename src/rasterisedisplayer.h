#ifndef RASTERISEDISPLAYER_H_INCLUDED
#define RASTERISEDISPLAYER_H_INCLUDED

#include "display.h"
#include "displayable.h"
#include "rasterise.h"

class RasteriseDisplayer : public IDisplayable
{
  public:
    RasteriseDisplayer(Display& display, Rasteriser& rasteriser);
    ~RasteriseDisplayer();

    virtual void set_extent(uint32_t width, uint32_t height) override;

    virtual void set_scene(Dispatcher& dispatcher, const Scene& scene) override;
    virtual void set_camera(Dispatcher& dispatcher, const Camera& camera) override;

    virtual void wait_idle() override;
    virtual void begin_render() override;
    virtual void end_render() override;

    virtual void get_debug_info(RenderDebugInfo& info) override;
    virtual void set_settings(const UiControlPanel& control_panel) override;
    virtual RenderType render_type() override { return RENDER_TYPE_RASTERISE; };

  private:
    virtual VkRenderPass get_render_pass() override;
    virtual VkCommandBuffer get_command_buffer() override;

    VkImage depth_image;
    VkImageView depth_image_view;
    VkDeviceMemory depth_image_memory;

    std::vector<VkFramebuffer> framebuffers;

    VkSemaphore image_semaphore;

    Display& display;
    Rasteriser& rasteriser;

    void create_framebuffers();
    void destroy_framebuffers();
    void create_image_semaphore();
    void create_depth_image();
    void destroy_depth_image();

    RasteriseDisplayer(RasteriseDisplayer const&) = delete;
    void operator=(RasteriseDisplayer const& x) = delete;
};

#endif