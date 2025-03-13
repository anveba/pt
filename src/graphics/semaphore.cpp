#include "semaphore.h"
#include "device.h"

Semaphore::Semaphore(Device& device, bool signaled)
    : device(device)
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    if (vkCreateSemaphore(device.logical_handle(), &semaphore_create_info, nullptr, &semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");
}

Semaphore::~Semaphore()
{
    vkDestroySemaphore(device.logical_handle(), semaphore, nullptr);
}