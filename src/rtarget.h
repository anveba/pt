#ifndef RTARGET_H_INCLUDED
#define RTARGET_H_INCLUDED

#include "util.h"

struct FramebufferInfo
{
    VkExtent2D extent;
    VkFormat format;
    VkRenderPass renderpass;
};

class IRenderTarget
{
  public:
    virtual VkFramebuffer acquire(VkSemaphore semaphore) = 0;
};

#endif