#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#include "util.h"

class Window;
class Rasteriser;
class Dispatcher;

class UserInterface
{
  public:
    UserInterface(Window& window, Dispatcher& dispatcher, Rasteriser& rasteriser);
    ~UserInterface();

    void new_frame();
    void render();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Rasteriser const* rasteriser;

    UserInterface(UserInterface const&) = delete;
    void operator=(UserInterface const&) = delete;
};

#endif
