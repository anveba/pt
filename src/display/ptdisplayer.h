#ifndef DISPLAY_PTDISPLAYER_H_INCLUDED
#define DISPLAY_PTDISPLAYER_H_INCLUDED

#include "display.h"
#include "displayable.h"
#include "pathtrace/pathtrace.h"

class PathTraceDisplayer : public IDisplayable
{
  public:
    PathTraceDisplayer(Display& display,
                       Dispatcher& dispatcher,
                       const Scene& scene,
                       VkExtent2D extent);
    ~PathTraceDisplayer();

    virtual void set_extent(uint32_t width, uint32_t height) override;

    virtual void set_scene(Dispatcher& dispatcher, const Scene& scene) override;
    virtual void set_camera(Dispatcher& dispatcher, const Camera& camera) override;

    virtual void wait_idle() override;
    virtual void begin_render() override;
    virtual void end_render() override;

    virtual void get_debug_info(RenderDebugInfo& info) override;
    virtual void set_settings(const UiControlPanel& control_panel) override;
    virtual RenderType render_type() override { return RENDER_TYPE_PATH_TRACE; };

  private:
    virtual VkRenderPass get_render_pass() override;
    virtual VkCommandBuffer get_command_buffer() override;

    VkRenderPass render_pass;
    VkSemaphore image_semaphore;

    std::vector<VkFramebuffer> framebuffers;

    Display& display;
    PathTracer path_tracer;

    void create_framebuffers();
    void destroy_framebuffers();
    void create_render_pass();
    void create_image_semaphore();

    NO_COPY(PathTraceDisplayer);
};

#endif