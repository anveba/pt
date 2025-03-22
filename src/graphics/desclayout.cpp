#include "desclayout.h"

DescriptorSetLayout::DescriptorSetLayout(Device& device,
                                         const VkDescriptorSetLayoutBinding* layout_bindings,
                                         uint32_t binding_count)
    : device(device)
{
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = binding_count;
    layout_info.pBindings = layout_bindings;

    if (vkCreateDescriptorSetLayout(device.logical_handle(), &layout_info, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout.");
}

DescriptorSetLayout::DescriptorSetLayout(Device& device, const std::vector<VkDescriptorSetLayoutBinding> layout_bindings)
    : DescriptorSetLayout(device, layout_bindings.data(), static_cast<uint32_t>(layout_bindings.size()))
{
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(device.logical_handle(), layout, nullptr);
}

VkDescriptorSetLayoutBinding DescriptorSetLayout::create_layout_binding(
    uint32_t binding,
    VkDescriptorType type,
    VkShaderStageFlags stage,
    uint32_t count,
    const VkSampler* samplers)
{
    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding = binding;
    layout_binding.descriptorType = type;
    layout_binding.descriptorCount = count;
    layout_binding.stageFlags = stage;
    layout_binding.pImmutableSamplers = samplers;
    return layout_binding;
}
