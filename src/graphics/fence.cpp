#include "fence.h"
#include "device.h"

Fence::Fence(Device& device, bool signaled)
    : device(device)
{
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    if (vkCreateFence(device.logical_handle(), &fence_create_info, nullptr, &fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create fence.");
}

void Fence::wait()
{
    vkWaitForFences(device.logical_handle(), 1, &fence, VK_TRUE, UINT64_MAX);
}

void Fence::reset()
{
    vkResetFences(device.logical_handle(), 1, &fence);
}

Fence::~Fence()
{
    vkDestroyFence(device.logical_handle(), fence, nullptr);
}