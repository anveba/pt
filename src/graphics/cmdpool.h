#ifndef GRAPHICS_CMDPOOL_H_INCLUDED
#define GRAPHICS_CMDPOOL_H_INCLUDED

#include "device.h"
#include "util.h"

class CommandPool
{
  public:
    CommandPool(Device& device);
    ~CommandPool();

    // TODO move and change so a caller can use their own command buffer
    void transfer_to_buffer(VkBuffer& buffer, const void* src_data, size_t size);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    VkCommandBuffer begin_one_time_use_command_buffer();
    void end_one_time_use_command_buffer(VkCommandBuffer command_buffer, VkQueue queue);

    // TODO move to its own class
    VkCommandBuffer create_command_buffer();
    void destroy_command_buffer(VkCommandBuffer command_buffer);

  private:
    Device& device;
    VkCommandPool command_pool;

    NO_COPY(CommandPool);
};

#endif