#include "storageimage.h"

StorageImage::StorageImage(Device& device,
                           CommandPool& command_pool,
                           VkExtent2D extent,
                           VkFormat format,
                           VkImageUsageFlagBits usage_flags,
                           VkImageLayout layout,
                           VkMemoryPropertyFlagBits memory_flags,
                           VkImageTiling tiling)
    : device(device)
    , extent(extent)
    , format(format)
    , usage_flags(usage_flags)
    , layout(layout)
    , memory_flags(memory_flags)
    , tiling(tiling)
{
    create(command_pool);
}

StorageImage::~StorageImage()
{
    destroy();
}

void StorageImage::rebuild(CommandPool& command_pool, VkExtent2D extent)
{
    destroy();
    this->extent = extent;
    create(command_pool);
}

void StorageImage::create(CommandPool& command_pool)
{
    image = device.create_image(extent.width,
                                extent.height,
                                format,
                                tiling,
                                usage_flags);

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device.logical_handle(), image, &mem_requirements);

    memory = device.allocate_memory(mem_requirements.size, mem_requirements.memoryTypeBits, memory_flags);

    vkBindImageMemory(device.logical_handle(), image, memory, 0);

    view = device.create_image_view(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer command_buffer = command_pool.begin_one_time_use_command_buffer();

    transition_image_layout(command_buffer,
                            image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            layout,
                            0,
                            0,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    command_pool.end_one_time_use_command_buffer(command_buffer, device.get_graphics_queue());
}

void StorageImage::destroy()
{
    vkDestroyImageView(device.logical_handle(), view, nullptr);
    vkDestroyImage(device.logical_handle(), image, nullptr);
    vkFreeMemory(device.logical_handle(), memory, nullptr);
}
