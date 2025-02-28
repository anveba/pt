#ifndef RENDERER_H_INCLUDED
#define RENDERER_H_INCLUDED

#include "util.h"

class Rasteriser;
class Dispatcher;
class Scene;
class IRenderTarget;
class Camera;
class Display;

class IRenderer
{
  public:
    virtual void set_scene(Dispatcher& dispatcher, const Scene& scene) = 0;
    virtual void set_camera(Dispatcher& dispatcher, const Camera& camera) = 0;

    virtual void begin_render(IRenderTarget* target) = 0;
    virtual VkSemaphore end_render() = 0;
};

class IDisplayable
{
  private:
    virtual VkRenderPass get_render_pass() = 0;
    virtual std::vector<VkImageView> get_extra_attachments() = 0;

    friend class FramebufferChain;
};

#endif