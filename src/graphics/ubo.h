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
        device.create_buffer(buffer, memory, sizeof(T), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(device.logical_handle(), memory, 0, sizeof(T), 0, (void**)&map);
    }

    ~UniformBuffer()
    {
        vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
        vkUnmapMemory(device.logical_handle(), memory);
        vkFreeMemory(device.logical_handle(), memory, nullptr);
    }

    inline const VkBuffer& handle() const { return buffer; }

    inline T* get_map() const { return map; }
    constexpr size_t size() const { return sizeof(T); }

  private:
    Device& device;

    VkBuffer buffer;
    VkDeviceMemory memory;
    T* map;

    NO_COPY(UniformBuffer);
};

#endif
