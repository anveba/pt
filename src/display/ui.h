#ifndef DISPLAY_UI_H_INCLUDED
#define DISPLAY_UI_H_INCLUDED

#include "displayable.h"
#include "graphics/descpool.h"
#include "lalgebra.h"
#include "util.h"

class Rasteriser;
class Dispatcher;
class Window;
class Device;

struct UiInfo
{
    float fps;
    Vec3 cam_position;
    Vec3 look_dir;
    float near, far;

    RenderDebugInfo render_info;
};

struct UiControlPanel
{
    RenderType render_type;
    uint32_t max_bounces;
    uint32_t samples_per_frame;
};

class UserInterface
{
  public:
    UserInterface(UiControlPanel& control_panel);

    void new_frame(const UiInfo& info);
    void render();

    static void init(Window& window);
    static void destroy();
    static void init_vulkan(DescriptorPool& descriptor_pool, IDisplayable& displayable);
    static void destroy_vulkan();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    IDisplayable* displayable;
    UiControlPanel* control_panel;
};

#endif
