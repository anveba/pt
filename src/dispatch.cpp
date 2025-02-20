#include "dispatch.h"

#include "util.h"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <set>

PhysicalDevice::PhysicalDevice(VkPhysicalDevice handle, VkSurfaceKHR surface)
    : handle(handle)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(handle, &properties);
    name = properties.deviceName;

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(handle, &features);

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(handle, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            graphics_family_idx = i;

        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(handle, i, surface, &present_support);
            if (present_support)
                present_family_idx = i;
        }
    }
}

SwapChainSupportDetails::SwapChainSupportDetails(VkPhysicalDevice device_handle, VkSurfaceKHR surface)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_handle, surface, &capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device_handle, surface, &format_count, nullptr);

    if (format_count > 0) {
        formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device_handle, surface, &format_count, formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device_handle, surface, &present_mode_count, nullptr);

    if (present_mode_count > 0) {
        present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device_handle, surface, &present_mode_count, present_modes.data());
    }
}

bool PhysicalDevice::is_suitable(const std::vector<const char*>& device_extensions, VkSurfaceKHR surface)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(handle, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(handle, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> unmet_extensions(device_extensions.begin(), device_extensions.end());

    for (const auto& extension : available_extensions)
        unmet_extensions.erase(extension.extensionName);

    bool swap_chain_is_suitable = false;
    if (unmet_extensions.empty()) {
        SwapChainSupportDetails swap_chain_support(handle, surface);
        swap_chain_is_suitable = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
    }

    return unmet_extensions.empty() &&
           swap_chain_is_suitable &&
           graphics_family_idx.has_value() &&
           present_family_idx.has_value();
}

static bool check_validation_layer_support(const std::vector<const char*>& validation_layers)
{
    uint32_t count;
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&count, nullptr));

    std::vector<VkLayerProperties> available(count);
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&count, available.data()));

    for (const char* layer : validation_layers) {
        bool found = false;
        for (const auto& properties : available) {
            if (!strcmp(layer, properties.layerName)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    return true;
}

static std::vector<const char*> get_required_instance_extensions(bool enable_validation_layers)
{
    unsigned int sdl_extension_count;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    assert(sdl_extensions != NULL);

    std::vector<const char*> extensions(sdl_extensions, sdl_extensions + sdl_extension_count);

    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

Dispatcher::Dispatcher(Window* window)
    : instance(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , device(VK_NULL_HANDLE)
{
    init_instance();
    if (window)
        set_window(*window);
    init_messenger();
    init_physical_device();
    init_logical_device();
    fetch_queues();
    if (window)
        create_swap_chain(*window);
    create_pipeline();
}

void Dispatcher::init_instance()
{
    if (enable_validation_layers && !check_validation_layer_support(validation_layers))
        throw std::runtime_error("Not all validation layers requested are available.");

    if (enable_validation_layers)
        std::cout << "Validation layers enabled." << std::endl;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    std::vector<const char*> extensions = get_required_instance_extensions(enable_validation_layers);
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    VK_ASSERT(vkCreateInstance(&create_info, nullptr, &instance));
}

// static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
//     VkDebugUtilsMessageSeverityFlagBitsEXT severity,
//     VkDebugUtilsMessageTypeFlagsEXT type,
//     const VkDebugUtilsMessengerCallbackDataEXT* data,
//     void* user_data)
// {
//     std::cerr << "Validation layer: " << data->pMessage << std::endl;
//     return VK_FALSE;
// }

void Dispatcher::init_messenger()
{
    // if (enable_validation_layers) {
    //     VkDebugUtilsMessengerCreateInfoEXT create_info{};
    //     create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    //     create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //     create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    //     create_info.pfnUserCallback = debug_callback;
    //     create_info.pUserData = nullptr;
    //     vkCreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &debug_messenger);
    // }
}

void Dispatcher::init_physical_device()
{
    uint32_t device_count;
    VK_ASSERT(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
    if (device_count == 0)
        throw std::runtime_error("Could not find devices with Vulkan support.");

    std::vector<VkPhysicalDevice> devices(device_count);
    VK_ASSERT(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));

    std::cout << "Found " << device_count << " physical devices:" << std::endl;
    physical_device = PhysicalDevice();
    for (auto d : devices) {
        PhysicalDevice d_info(d, surface);
        std::cout << "    " << d_info.name << std::endl;

        if (physical_device.is_empty() && d_info.is_suitable(device_extensions, surface))
            physical_device = d_info;
    }

    if (physical_device.is_empty())
        throw std::runtime_error("No suitable device found.");

    std::cout << "Using physical device " << physical_device.name << "." << std::endl;
}

void Dispatcher::init_logical_device()
{
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {
        physical_device.graphics_family_idx.value(),
        physical_device.present_family_idx.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queue_family_idx : unique_queue_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = queue_family_idx;
        qci.queueCount = 1;
        qci.pQueuePriorities = &queuePriority;
        queue_create_infos.push_back(qci);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pEnabledFeatures = &deviceFeatures;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physical_device.handle, &create_info, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device.");
}

void Dispatcher::fetch_queues()
{
    vkGetDeviceQueue(device, physical_device.graphics_family_idx.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, physical_device.present_family_idx.value(), 0, &present_queue);
}

void Dispatcher::create_swap_chain(Window& window)
{
    SwapChainSupportDetails swap_chain_support(physical_device.handle, surface);

    VkSurfaceFormatKHR surface_format = choose_surface_format(swap_chain_support.formats);
    swap_chain.image_format = surface_format.format;
    VkPresentModeKHR present_mode = choose_present_mode(swap_chain_support.present_modes);
    swap_chain.extent = choose_extent(window, swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount;
    if (swap_chain_support.capabilities.maxImageCount == 0 || swap_chain_support.capabilities.maxImageCount > image_count)
        image_count += 1;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swap_chain.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = { physical_device.graphics_family_idx.value(),
                                        physical_device.present_family_idx.value() };
    if (physical_device.graphics_family_idx != physical_device.present_family_idx) {
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

    if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain.handle) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swap chain.");

    vkGetSwapchainImagesKHR(device, swap_chain.handle, &image_count, nullptr);
    swap_chain.images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain.handle, &image_count, swap_chain.images.data());

    swap_chain.image_views.resize(swap_chain.images.size());
    for (size_t i = 0; i < swap_chain.images.size(); i++) {

        // TODO use generalised function
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swap_chain.images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swap_chain.image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &create_info, nullptr, &swap_chain.image_views[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image views!");
    }
}

VkSurfaceFormatKHR Dispatcher::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
    if (formats.empty())
        throw std::runtime_error("No surface formats to choose from.");

    for (const VkSurfaceFormatKHR& format : formats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;

    return formats[0];
}

VkPresentModeKHR Dispatcher::choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes)
{
    for (const auto& present_mode : present_modes)
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return present_mode;

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Dispatcher::choose_extent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_GetWindowSizeInPixels(window.handle, &width, &height);

        VkExtent2D extent{};
        extent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

void Dispatcher::create_pipeline()
{
}

Dispatcher::~Dispatcher()
{
    if (swap_chain.handle != VK_NULL_HANDLE) {
        for (auto view : swap_chain.image_views)
            vkDestroyImageView(device, view, nullptr);
        vkDestroySwapchainKHR(device, swap_chain.handle, nullptr);
    }
    vkDeviceWaitIdle(device);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void Dispatcher::set_window(Window& window)
{
    if (!SDL_Vulkan_CreateSurface(window.handle, instance, nullptr, &surface))
        throw std::runtime_error("Failed to create surface: " + std::string(SDL_GetError()));
}