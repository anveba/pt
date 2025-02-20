#ifndef DISPATCH_H_INCLUDED
#define DISPATCH_H_INCLUDED

#include "renderer.h"
#include "shader.h"
#include "util.h"
#include "window.h"

#include <optional>
#include <string>
#include <vector>

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    SwapChainSupportDetails(VkPhysicalDevice device_handle, VkSurfaceKHR surface);
};

struct SwapChain
{
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkFormat image_format;
    VkExtent2D extent;

    SwapChain()
        : handle(VK_NULL_HANDLE)
    {
    }
};

struct PhysicalDevice
{
    VkPhysicalDevice handle;
    std::string name;

    std::optional<uint32_t> graphics_family_idx;
    std::optional<uint32_t> present_family_idx;

    inline bool is_empty() { return handle == VK_NULL_HANDLE; };

    PhysicalDevice()
        : handle(VK_NULL_HANDLE)
    {
    }
    PhysicalDevice(VkPhysicalDevice handle, VkSurfaceKHR surface);

    bool is_suitable(const std::vector<const char*>& device_extensions, VkSurfaceKHR surface);
};

class Dispatcher
{
  public:
    Dispatcher(Window* window);
    ~Dispatcher();

  private:
    VkInstance instance;
    VkSurfaceKHR surface;
    PhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    SwapChain swap_chain;

    friend class Shader;
    friend class Rasteriser;
    friend class UserInterface;

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    void init_instance();
    void set_window(Window& window);
    void init_messenger();
    void init_physical_device();
    void init_logical_device();
    void fetch_queues();
    void create_swap_chain(Window& window);

    void create_pipeline();

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes);
    VkExtent2D choose_extent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities);

    Dispatcher(Dispatcher const&) = delete;
    void operator=(Dispatcher const&) = delete;
};

#endif