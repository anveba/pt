#ifndef GRAPHICS_SHADER_H_INCLUDED
#define GRAPHICS_SHADER_H_INCLUDED

#include <string>

#include "util.h"

class Device;

class Shader
{
  public:
    Shader(Device& device, const std::string& path);
    ~Shader();

  private:
    VkShaderModule shader_module;
    Device& device;

    friend class Rasteriser;
    friend class PathTracer;

    NO_COPY(Shader);
};

#endif