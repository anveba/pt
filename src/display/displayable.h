#ifndef DISPLAYABLE_H_INCLUDED
#define DISPLAYABLE_H_INCLUDED

#include "util.h"

class Dispatcher;
class Scene;
class Camera;
struct UiControlPanel;

struct RenderDebugInfo
{
    uint32_t samples;
};

constexpr int MAX_RENDER_TYPE = 2;

class IDisplayable
{
  public:
    virtual void set_extent(uint32_t width, uint32_t height) = 0;

    virtual void set_scene(const Scene& scene) = 0;
    virtual void set_camera(const Camera& camera) = 0;

    virtual void wait_idle() = 0;
    virtual void begin_render() = 0;
    virtual void end_render() = 0;

    virtual void get_debug_info(RenderDebugInfo& info) = 0;
    virtual void set_settings(const UiControlPanel& control_panel) = 0;
    virtual RenderType render_type() = 0;

  private:
    virtual VkRenderPass get_render_pass() = 0;
    virtual VkCommandBuffer get_command_buffer() = 0;

    friend class UserInterface;
};

#endif