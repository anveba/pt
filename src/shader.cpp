#include "shader.h"

#include "dispatch.h"
#include "util.h"

#include <cassert>

Shader::Shader(Dispatcher& dispatcher, const std::string& path)
    : dispatcher(&dispatcher)
{
    std::vector<char> code = read_bytes(path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(dispatcher.device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module. Path: " + path);
}

Shader::~Shader()
{
    vkDestroyShaderModule(dispatcher->device, shader_module, nullptr);
}