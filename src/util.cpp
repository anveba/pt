#include "util.h"

#include <fstream>

std::vector<char> read_bytes(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to open path: " + path);

    size_t sz = (size_t)file.tellg();
    std::vector<char> buffer(sz);

    file.seekg(0);
    file.read(buffer.data(), sz);
    file.close();

    return buffer;
}

void transition_image_layout(
    VkCommandBuffer command_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    const VkImageSubresourceRange& subresource_range)
{
    VkImageMemoryBarrier image_memory_barrier{};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.oldLayout = old_layout;
    image_memory_barrier.newLayout = new_layout;
    image_memory_barrier.srcAccessMask = src_access;
    image_memory_barrier.dstAccessMask = dst_access;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}

void copy_image(
    VkCommandBuffer command_buffer,
    VkImage src,
    VkImageLayout src_layout,
    VkImage dst,
    VkImageLayout dst_layout,
    uint32_t width,
    uint32_t height)
{
    VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    transition_image_layout(command_buffer,
                            dst,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            src,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    VkImageCopy copy{};
    copy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.extent = { width, height, 1 };
    copy.srcOffset = { 0, 0, 0 };
    copy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copy.dstOffset = { 0, 0, 0 };
    vkCmdCopyImage(command_buffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transition_image_layout(command_buffer,
                            dst,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            dst_layout,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            src,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            src_layout,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            subresource_range);
}