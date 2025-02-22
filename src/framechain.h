#ifndef FRAMECHAIN_H_INCLUDED
#define FRAMECHAIN_H_INCLUDED

#include "display.h"
#include "renderer.h"
#include "rtarget.h"
#include "util.h"

class FramebufferChain : public IRenderTarget
{
  public:
    FramebufferChain(Display& display, IRenderer* renderer);
    ~FramebufferChain();

    VkFramebuffer acquire(VkSemaphore semaphore);

  private:
    std::vector<VkFramebuffer> framebuffers;

    Display* const display;
};

#endif