#include "shader.h"

#include "device.h"
#include "util.h"

#include <cassert>

Shader::Shader(Device& device, const std::string& path)
    : device(device)
{
    std::vector<char> code = read_bytes(path);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device.logical_handle(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module. Path: " + path);
}

Shader::~Shader()
{
    vkDestroyShaderModule(device.logical_handle(), shader_module, nullptr);
}