#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include <optional>

#include "context.h"
#include "window.h"

enum DeviceUsage
{
    DEVICE_USAGE_MINIMUM = 0,
    DEVICE_USAGE_WINDOW_BIT = 1,
    DEVICE_USAGE_RAY_TRACE_BIT = 2,
};

struct PhysicalDeviceInfo
{
    std::string name;
    VkPhysicalDeviceType type;
    VkDeviceSize heap_size;

    std::optional<uint32_t> graphics_family_idx;
    std::optional<uint32_t> present_family_idx;
};

struct SwapChainSupport
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class Device
{
  public:
    Device(VulkanContext& context, DeviceUsage usage, Window* window = nullptr);
    ~Device();

    inline const SwapChainSupport& get_swap_chain_support() { return swap_chain_support; };

    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask);

    uint32_t find_suitable_memory_type(int32_t typeFilter,
                                       VkMemoryPropertyFlags desired_flags);
    VkFormat find_image_format(const std::vector<VkFormat>& desirable,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  private:
    VkPhysicalDevice physical;
    PhysicalDeviceInfo physical_device_info;
    SwapChainSupport swap_chain_support;

    VkDevice logical;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;

    VulkanContext const* context;
    const DeviceUsage usage;

    void create_image(VkImage& image,
                      VkDeviceMemory& memory,
                      uint32_t width,
                      uint32_t height,
                      VkFormat format,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VkMemoryPropertyFlags properties);

    void create_buffer(VkBuffer& buffer,
                       VkDeviceMemory& memory,
                       VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags mem_flags);

    void init_physical_device(VkPhysicalDevice handle, VkSurfaceKHR surface);
    void find_physical_device(const std::vector<VkPhysicalDevice>& devices, DeviceUsage usage, VkSurfaceKHR surface);

    void init_logical_device(VulkanContext& context, DeviceUsage usage);

    friend class Display;
    friend class Shader;
    friend class Rasteriser;
    friend class Dispatcher;
    friend class UserInterface;
    friend class FramebufferChain;

    Device(Device const&) = delete;
    void operator=(Device const&) = delete;
};

#endif