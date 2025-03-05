#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <iostream>
#include <type_traits>
#include <vector>
#include <vulkan.h>

#define VK_ASSERT(e)                                                                        \
    {                                                                                       \
        static_assert(std::is_same<decltype(e), VkResult>::value, "type must be VkResult"); \
        if (e) {                                                                            \
            std::cerr << "Vulkan error: " << e << std::endl;                                \
            exit(1);                                                                        \
        }                                                                                   \
    }

std::vector<char> read_bytes(const std::string& path);

template<typename T>
inline T round_up_to(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
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
    const VkImageSubresourceRange& subresource_range);

#endif
