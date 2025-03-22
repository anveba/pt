#ifndef GRAPHICS_SAMPLER_H_INCLUDED
#define GRAPHICS_SAMPLER_H_INCLUDED

#include "device.h"
#include "util.h"

class Sampler
{
  public:
    Sampler(Device& device);
    ~Sampler();

    inline const VkSampler& handle() const { return sampler; }

  private:
    Device& device;
    VkSampler sampler;

    NO_COPY(Sampler);
};

#endif