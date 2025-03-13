#ifndef GRAPHICS_FENCE_H_INCLUDED
#define GRAPHICS_FENCE_H_INCLUDED

#include "util.h"

class Device;

class Fence
{
  public:
    Fence(Device& device, bool signaled);
    ~Fence();

    inline const VkFence& handle() const { return fence; }

    void wait();
    void reset();

  private:
    Device& device;
    VkFence fence;

    NO_COPY(Fence);
};

#endif