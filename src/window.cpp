#include "window.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

#include <imgui_impl_sdl3.h>

Window::Window(uint32_t width, uint32_t height)
    : width(width)
    , height(height)
{
    assert(width > 0 && width <= std::numeric_limits<int>::max());
    assert(height > 0 && height <= std::numeric_limits<int>::max());

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    handle = SDL_CreateWindow(
        "Vulkan Test",
        width,
        height,
        window_flags);
    if (handle == NULL)
        throw std::runtime_error("Failed to open window" + std::string(SDL_GetError()));
}

Window::~Window()
{
    SDL_DestroyWindow(handle);
}

void Window::process_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            exit(0);
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            event.key;
        }
    }
}
