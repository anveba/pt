#ifndef SHADER_H_INCLUDED
#define SHADER_H_INCLUDED

#include <string>

#include "renderer.h"
#include "util.h"

class Dispatcher;

class Shader
{
  public:
    Shader(Dispatcher& dispatcher, const std::string& path);
    ~Shader();

  private:
    Shader(Shader const&) = delete;
    void operator=(Shader const&) = delete;

    VkShaderModule shader_module;
    Dispatcher* dispatcher; // TODO, remove raw pointer

    friend class Rasteriser;
};

#endif