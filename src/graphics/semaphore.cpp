#include "semaphore.h"
#include "device.h"

Semaphore::Semaphore(Device& device)
    : device(device)
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device.logical_handle(), &semaphore_create_info, nullptr, &semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");
}

Semaphore::~Semaphore()
{
    vkDestroySemaphore(device.logical_handle(), semaphore, nullptr);
}