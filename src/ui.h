#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#include "lalgebra.h"
#include "util.h"

class Window;
class Rasteriser;
class Dispatcher;

struct UiInfo
{
    float fps;
    Vec3 cam_position;
    Vec3 look_dir;
    float far, near;
};

class UserInterface
{
  public:
    UserInterface(Window& window, Dispatcher& dispatcher, Rasteriser& rasteriser);
    ~UserInterface();

    void new_frame(const UiInfo& info);
    void render();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

  private:
    Rasteriser const* rasteriser;

    UserInterface(UserInterface const&) = delete;
    void operator=(UserInterface const&) = delete;
};

#endif
