#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

#include "device.h"
#include "rtarget.h"
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
            VkFormat depth_format,
            VkPresentModeKHR present_mode,
            VkExtent2D extent);
    ~Display();

    void present(VkSemaphore wait_for);

  private:
    SwapChain swap_chain;

    uint32_t current_image_index;

    Device* const device;

    uint32_t acquire_next_index(VkSemaphore image_ready);
    void create_swap_chain(Device& device,
                           Window& window,
                           VkSurfaceFormatKHR surface_format,
                           VkPresentModeKHR present_mode,
                           VkExtent2D extent);

    friend class FramebufferChain;

    Display(Display const&) = delete;
    void operator=(Display const&) = delete;
};

#endif