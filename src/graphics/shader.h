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

    const VkShaderModule& handle() const { return shader_module; }

  private:
    VkShaderModule shader_module;
    Device& device;

    NO_COPY(Shader);
};

#endif