#ifndef SHADER_H_INCLUDED
#define SHADER_H_INCLUDED

#include <string>

#include "renderer.h"
#include "util.h"

class Device;

class Shader
{
  public:
    Shader(Device& device, const std::string& path);
    ~Shader();

  private:
    Shader(Shader const&) = delete;
    void operator=(Shader const&) = delete;

    VkShaderModule shader_module;
    Device const* device;

    friend class Rasteriser;
};

#endif