#include "window.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

#include <SDL3/SDL_vulkan.h>
#include <imgui_impl_sdl3.h>

Window::Window(VulkanContext& context, uint32_t width, uint32_t height)
    : width(width)
    , height(height)
    , context(&context)
{
    assert(width > 0 && width <= std::numeric_limits<int>::max());
    assert(height > 0 && height <= std::numeric_limits<int>::max());

    if (!(context.usage & CONTEXT_USAGE_WINDOW_BIT))
        throw std::runtime_error("Windowing is not enabled in Vulkan context.");

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    handle = SDL_CreateWindow(
        "Vulkan Test",
        width,
        height,
        window_flags);
    if (handle == NULL)
        throw std::runtime_error("Failed to open window" + std::string(SDL_GetError()));

    if (!SDL_Vulkan_CreateSurface(handle, context.instance, nullptr, &surface))
        throw std::runtime_error("Failed to create surface: " + std::string(SDL_GetError()));
}

Window::~Window()
{
    vkDestroySurfaceKHR(context->instance, surface, nullptr);
    SDL_DestroyWindow(handle);
}

void Window::get_extent(uint32_t& width, uint32_t& height)
{
    int w, h;
    SDL_GetWindowSizeInPixels(handle, &w, &h);
    width = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);
}

void Window::process_events(IInputHandler* input_handler)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            exit(0);
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (input_handler)
                input_handler->key_down(event.key.scancode);
        } else if (event.type == SDL_EVENT_KEY_UP) {
            if (input_handler)
                input_handler->key_up(event.key.scancode);
        }
    }
}
