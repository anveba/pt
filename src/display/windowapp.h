#ifndef DISPLAY_WINDOWAPP_H_INCLUDED
#define DISPLAY_WINDOWAPP_H_INCLUDED

#include "graphics/dispatch.h"
#include "scene/scene.h"
#include "ui.h"
#include "util.h"

class WindowedApplication
{
  public:
    WindowedApplication(uint32_t width, uint32_t height, const std::vector<const char*>& validation_layers);
    ~WindowedApplication();

    void begin(const Scene& scene, Camera& camera);

  private:
    VulkanContext context;
    Window window;
    Device device;
    Dispatcher dispatcher;
    UiControlPanel control_panel;

    NO_COPY(WindowedApplication);
};

#endif