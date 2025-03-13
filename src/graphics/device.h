#ifndef GRAPHICS_DEVICE_H_INCLUDED
#define GRAPHICS_DEVICE_H_INCLUDED

#include <optional>

#include "context.h"
#include "display/window.h"

enum DeviceUsage
{
    DEVICE_USAGE_MINIMUM = 0,
    DEVICE_USAGE_WINDOW_BIT = 1,
    DEVICE_USAGE_RAY_TRACE_BIT = 2,
};

struct PhysicalDeviceInfo
{
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;

    VkDeviceSize heap_size;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_properties;

    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features;
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features;

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

    void query_swap_chain_support(SwapChainSupport& support, const Window& window);

    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask);

    uint32_t find_suitable_memory_type(int32_t typeFilter,
                                       VkMemoryPropertyFlags desired_flags);
    VkFormat find_image_format(const std::vector<VkFormat>& desirable,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

    inline const PhysicalDeviceInfo& get_physical_device_info() { return physical_device_info; }

    inline const VkDevice& logical_handle() const { return logical; }
    inline const VkPhysicalDevice& physical_handle() const { return physical; }

    inline void wait_idle() { vkDeviceWaitIdle(logical); }

  private:
    VkPhysicalDevice physical;
    PhysicalDeviceInfo physical_device_info;

    VkDevice logical;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;

    VulkanContext& context;
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
                       VkMemoryPropertyFlags mem_flags,
                       VkMemoryAllocateFlags alloc_flags = 0);

    void init_physical_device(VkPhysicalDevice handle, VkSurfaceKHR surface);
    void find_physical_device(const std::vector<VkPhysicalDevice>& devices, DeviceUsage usage, VkSurfaceKHR surface);

    void init_logical_device(VulkanContext& context, DeviceUsage usage);

    // TODO remove
    friend class Display;
    friend class DescriptorPool;
    friend class Shader;
    friend class Rasteriser;
    friend class PathTracer;
    friend class Dispatcher;
    friend class CommandPool;
    friend class UserInterface;
    friend class RasteriseDisplayer;
    friend class PathTraceDisplayer;
    friend class DescriptorSetLayout;

    NO_COPY(Device);
};

#endif