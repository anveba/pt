#ifndef DISPLAY_WINDOW_H_INCLUDED
#define DISPLAY_WINDOW_H_INCLUDED

#include <cstdint>

#include "graphics/context.h"
#include "input.h"
#include <SDL3/SDL.h>

struct WindowEventInfo
{
    bool resize;
    bool exit;
};

class Window
{
  public:
    Window(VulkanContext& context, uint32_t width, uint32_t height);
    ~Window();

    uint32_t get_pixel_width() { return pixel_width; }
    uint32_t get_pixel_height() { return pixel_height; }
    uint32_t get_width() { return width; }
    uint32_t get_height() { return height; }

    void process_events(WindowEventInfo& info, IInputHandler* input_handler);

    inline const VkSurfaceKHR surface_handle() const { return surface; }

  private:
    SDL_Window* handle;
    uint32_t width, height;
    uint32_t pixel_width, pixel_height;
    VkSurfaceKHR surface;

    VulkanContext& context;

    void query_dimensions();

    friend class UserInterface;

    NO_COPY(Window);
};

#endif
