#ifndef WINDOW_H_INCLUDED
#define WINDOW_H_INCLUDED

#include <cstdint>

#include <SDL3/SDL.h>

class Window
{
  public:
    Window(uint32_t width, uint32_t height);
    ~Window();

    void process_events();

  private:
    SDL_Window* handle;
    uint32_t width, height;

    Window(Window const&) = delete;
    void operator=(Window const&) = delete;

    friend class Dispatcher;
    friend class UserInterface;
};

#endif
