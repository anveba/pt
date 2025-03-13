#ifndef GRAPHICS_SEMAPHORE_H_INCLUDED
#define GRAPHICS_SEMAPHORE_H_INCLUDED

#include "util.h"

class Device;

class Semaphore
{
  public:
    Semaphore(Device& device, bool signaled);
    ~Semaphore();
    inline const VkSemaphore& handle() const { return semaphore; }

  private:
    Device& device;
    VkSemaphore semaphore;

    NO_COPY(Semaphore);
};

#endif