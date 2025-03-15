#ifndef GRAPHICS_UBO_H_INCLUDED
#define GRAPHICS_UBO_H_INCLUDED

#include "device.h"

template<typename T>
class UniformBuffer
{
  public:
    UniformBuffer(Device& device)
        : device(device)
    {
        device.create_buffer(uniform_buffer, uniform_buffer_memory, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(device.logical_handle(), uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, (void**)&uniform_map);
    }

    ~UniformBuffer()
    {
        vkDestroyBuffer(device.logical_handle(), uniform_buffer, nullptr);
        vkUnmapMemory(device.logical_handle(), uniform_buffer_memory);
        vkFreeMemory(device.logical_handle(), uniform_buffer_memory, nullptr);
    }

    inline T* get_map() const { return map; }

  private:
    Device& device;
    T* map;

    NO_COPY(UniformBuffer);
};

#endif
