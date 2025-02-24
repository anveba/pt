#ifndef WINDOW_H_INCLUDED
#define WINDOW_H_INCLUDED

#include <cstdint>

#include "context.h"
#include "input.h"
#include <SDL3/SDL.h>

class Window
{
  public:
    Window(VulkanContext& context, uint32_t width, uint32_t height);
    ~Window();

    void get_extent(uint32_t& width, uint32_t& height);

    void process_events(IInputHandler* input_handler);

  private:
    SDL_Window* handle;
    uint32_t width, height;
    VkSurfaceKHR surface;
    VulkanContext const* context;

    Window(Window const&) = delete;
    void operator=(Window const&) = delete;

    friend class Device;
    friend class Display;
    friend class UserInterface;
};

#endif
