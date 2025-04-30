#ifndef PATHTRACE_PTDISPLAYER_H_INCLUDED
#define PATHTRACE_PTDISPLAYER_H_INCLUDED

#include "compute/tonemap.h"
#include "display/display.h"
#include "display/displayable.h"
#include "graphics/cmdpool.h"
#include "graphics/descpool.h"
#include "graphics/fence.h"
#include "graphics/storageimage.h"
#include "pathtrace.h"

class PathTraceDisplayer : public IDisplayable
{
  public:
    PathTraceDisplayer(Display& display,
                       DescriptorPool& descriptor_pool,
                       CommandPool& command_pool,
                       const Scene& scene,
                       VkExtent2D extent);
    ~PathTraceDisplayer();

    virtual void set_extent(uint32_t width, uint32_t height) override;

    virtual void set_scene(const Scene& scene) override;
    virtual void set_camera(const Camera& camera) override;

    virtual void wait_idle() override;
    virtual void begin_render() override;
    virtual void end_render() override;

    virtual void get_debug_info(RenderDebugInfo& info) const override;
    virtual void set_settings(const UiControlPanel& control_panel) override;
    virtual RenderType render_type() const override { return RENDER_TYPE_PATH_TRACE; };
    virtual size_t max_in_flight() const override { return PathTracer::IN_FLIGHT; };

  private:
    virtual VkRenderPass get_render_pass() override;
    virtual VkCommandBuffer get_command_buffer() override;

    VkRenderPass render_pass;

    size_t current_frame;
    InitableArray<Semaphore, PathTracer::IN_FLIGHT> image_semaphores;
    InitableArray<Semaphore, PathTracer::IN_FLIGHT> render_semaphores;
    InitableArray<Fence, PathTracer::IN_FLIGHT> render_fences;

    StorageImage accumulation_image;
    StorageImage intermediate_image;

    std::vector<VkFramebuffer> framebuffers;

    Display& display;
    CommandPool& command_pool;
    PathTracer path_tracer;
    ToneMapper<PathTracer::IN_FLIGHT> tone_mapper;
    std::array<VkCommandBuffer, PathTracer::IN_FLIGHT> command_buffers;

    bool in_render;

    void create_framebuffers();
    void destroy_framebuffers();
    void create_render_pass();

    void blit_result(VkCommandBuffer command_buffer, VkImage image, uint32_t width, uint32_t height);

    NO_COPY(PathTraceDisplayer);
};

#endif