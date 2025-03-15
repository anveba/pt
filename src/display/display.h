#ifndef DISPLAY_DISPLAY_H_INCLUDED
#define DISPLAY_DISPLAY_H_INCLUDED

#include "graphics/device.h"
#include "graphics/semaphore.h"
#include "util.h"
#include "window.h"

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

VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats);
VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes);
VkExtent2D choose_extent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities);
VkFormat choose_depth_format(Device& device);

class Display
{
  public:
    Display(Device& device,
            Window& window,
            VkSurfaceFormatKHR format,
            VkPresentModeKHR present_mode);
    ~Display();

    void present(Semaphore& wait_for);

    uint32_t acquire_next_index(Semaphore& image_ready);

    void copy_image(VkImage src, uint32_t dst_index);

    inline VkExtent2D get_extent() { return swap_chain.extent; }
    void recreate_swap_chain();

    inline Device& get_device() { return device; }
    inline Window& get_window() { return window; }
    inline const SwapChain& get_swap_chain() { return swap_chain; }
    inline const VkSurfaceFormatKHR& get_surface_format() { return surface_format; }

  private:
    SwapChain swap_chain;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;

    uint32_t current_image_index;

    Device& device;
    Window& window;

    void create_swap_chain(Device& device,
                           Window& window,
                           VkSurfaceFormatKHR surface_format,
                           VkPresentModeKHR present_mode);

    void destroy_swap_chain();

    NO_COPY(Display);
};

#endif