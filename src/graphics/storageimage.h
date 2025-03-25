#ifndef GRAPHICS_STORAGEIMAGE_H_INCLUDED
#define GRAPHICS_STORAGEIMAGE_H_INCLUDED

#include "cmdpool.h"

class StorageImage
{
  public:
    StorageImage(Device& device,
                 CommandPool& command_pool,
                 VkExtent2D extent,
                 VkFormat format,
                 VkImageUsageFlagBits usage_flags,
                 VkImageLayout layout,
                 VkMemoryPropertyFlagBits memory_flags,
                 VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);
    ~StorageImage();

    void rebuild(CommandPool& command_pool, VkExtent2D extent);

    inline const VkImage& handle() const { return image; }
    inline const VkImageView& get_view() const { return view; }
    inline const VkDeviceMemory& get_memory() const { return memory; }
    inline const VkExtent2D& get_extent() const { return extent; }
    inline const VkFormat& get_format() const { return format; }

  private:
    Device& device;
    VkExtent2D extent;
    VkFormat format;
    VkImageUsageFlagBits usage_flags;
    VkImageLayout layout;
    VkMemoryPropertyFlagBits memory_flags;
    VkImageTiling tiling;

    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;

    void create(CommandPool& command_pool);
    void destroy();
};

#endif