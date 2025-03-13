#include "display.h"
#include <algorithm>
#include <limits>

Display::Display(Device& device,
                 Window& window,
                 VkSurfaceFormatKHR surface_format,
                 VkPresentModeKHR present_mode)
    : surface_format(surface_format)
    , present_mode(present_mode)
    , device(device)
    , window(window)
{
    SwapChainSupport swap_chain_support;
    device.query_swap_chain_support(swap_chain_support, window);
    create_swap_chain(device, window, surface_format, present_mode);
}

Display::~Display()
{
    destroy_swap_chain();
}

void Display::destroy_swap_chain()
{
    for (auto view : swap_chain.image_views)
        vkDestroyImageView(device.logical, view, nullptr);
    vkDestroySwapchainKHR(device.logical, swap_chain.handle, nullptr);
}

VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
    if (formats.empty())
        throw std::runtime_error("No surface formats to choose from.");

    for (const VkSurfaceFormatKHR& format : formats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;

    return formats[0];
}

VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
    for (const auto& present_mode : present_modes)
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_extent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        uint32_t width = window.get_pixel_width(), height = window.get_pixel_height();

        VkExtent2D extent{};
        extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

VkFormat choose_depth_format(Device& device)
{
    return device.find_image_format(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Display::create_swap_chain(Device& device,
                                Window& window,
                                VkSurfaceFormatKHR surface_format,
                                VkPresentModeKHR present_mode)
{
    SwapChainSupport swap_chain_support;
    device.query_swap_chain_support(swap_chain_support, window);

    swap_chain.image_format = surface_format.format;
    swap_chain.extent = choose_extent(window, swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount;
    if (swap_chain_support.capabilities.maxImageCount == 0 ||
        swap_chain_support.capabilities.maxImageCount > image_count) {
        image_count += 1;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = window.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swap_chain.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TODO TRANSFER_DST_BIT is used for ray tracing and should be only used when indicated

    uint32_t queue_family_indices[] = { device.physical_device_info.graphics_family_idx.value(),
                                        device.physical_device_info.present_family_idx.value() };
    if (device.physical_device_info.graphics_family_idx != device.physical_device_info.present_family_idx) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE; // TODO
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device.logical, &create_info, nullptr, &swap_chain.handle) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swap chain.");

    vkGetSwapchainImagesKHR(device.logical, swap_chain.handle, &image_count, nullptr);
    swap_chain.images.resize(image_count);
    vkGetSwapchainImagesKHR(device.logical, swap_chain.handle, &image_count, swap_chain.images.data());

    swap_chain.image_views.resize(swap_chain.images.size());
    for (size_t i = 0; i < swap_chain.images.size(); i++)
        swap_chain.image_views[i] = device.create_image_view(swap_chain.images[i], swap_chain.image_format, VK_IMAGE_ASPECT_COLOR_BIT);
}

uint32_t Display::acquire_next_index(Semaphore& image_ready)
{
    VkResult result = vkAcquireNextImageKHR(device.logical, swap_chain.handle, UINT64_MAX, image_ready.handle(), VK_NULL_HANDLE, &current_image_index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swap chain image.");

    return current_image_index;
}

void Display::recreate_swap_chain()
{
    vkDeviceWaitIdle(device.logical);
    destroy_swap_chain();

    create_swap_chain(device, window, surface_format, present_mode);
}

void Display::present(Semaphore& wait_for)
{
    VkSemaphore signal_semaphores[] = { wait_for.handle() };

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { swap_chain.handle };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &current_image_index;

    present_info.pResults = nullptr;

    vkQueuePresentKHR(device.present_queue, &present_info);
}