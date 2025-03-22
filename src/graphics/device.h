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
    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties;

    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features;
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features;

    std::optional<uint32_t> graphics_family_idx;
    std::optional<uint32_t> present_family_idx;
    std::optional<uint32_t> compute_family_idx;
    std::optional<uint32_t> transfer_family_idx;
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

    uint32_t find_suitable_memory_type(int32_t typeFilter,
                                       VkMemoryPropertyFlags desired_flags);
    VkFormat find_image_format(const std::vector<VkFormat>& desirable,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

    inline const PhysicalDeviceInfo& get_physical_device_info() { return physical_device_info; }

    VkDeviceAddress get_buffer_address(VkBuffer buffer);

    inline const VkDevice& logical_handle() const { return logical; }
    inline const VkPhysicalDevice& physical_handle() const { return physical; }

    inline const VkQueue& get_graphics_queue() const { return graphics_queue; }
    inline const VkQueue& get_present_queue() const { return present_queue; }
    inline const VkQueue& get_compute_queue() const { return compute_queue; }

    inline VulkanContext& get_context() const { return context; }

    inline void wait_idle() { vkDeviceWaitIdle(logical); }

    VkImage create_image(uint32_t width,
                         uint32_t height,
                         VkFormat format,
                         VkImageTiling tiling,
                         VkImageUsageFlags usage);

    VkDeviceMemory allocate_memory(VkDeviceSize size, uint32_t memory_type_bits, VkMemoryPropertyFlags properties);

    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask);

    void create_buffer(VkBuffer& buffer,
                       VkDeviceMemory& memory,
                       VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags mem_flags,
                       VkMemoryAllocateFlags alloc_flags = 0);

  private:
    VkPhysicalDevice physical;
    PhysicalDeviceInfo physical_device_info;

    VkDevice logical;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    VulkanContext& context;
    const DeviceUsage usage;

    void init_physical_device(VkPhysicalDevice handle, VkSurfaceKHR surface);
    void find_physical_device(const std::vector<VkPhysicalDevice>& devices, DeviceUsage usage, VkSurfaceKHR surface);

    void init_logical_device(VulkanContext& context, DeviceUsage usage);

    NO_COPY(Device);
};

#endif