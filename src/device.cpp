#include "device.h"

#include <set>

static void query_swap_chain_support(SwapChainSupport& support, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

    if (format_count > 0) {
        support.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, support.formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

    if (present_mode_count > 0) {
        support.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, support.present_modes.data());
    }
}

Device::Device(VulkanContext& context, DeviceUsage usage, Window* window)
    : graphics_queue(VK_NULL_HANDLE)
    , present_queue(VK_NULL_HANDLE)
    , compute_queue(VK_NULL_HANDLE)
    , context(&context)
    , usage(usage)
{
    if (((usage & DEVICE_USAGE_WINDOW_BIT) != 0) == (window == nullptr))
        throw std::runtime_error("Window usage bit is not compatible with window parameter given.");

    uint32_t device_count;
    VK_ASSERT(vkEnumeratePhysicalDevices(context.instance, &device_count, nullptr));
    if (device_count == 0)
        throw std::runtime_error("Could not find devices with Vulkan support.");

    std::vector<VkPhysicalDevice> devices(device_count);
    VK_ASSERT(vkEnumeratePhysicalDevices(context.instance, &device_count, devices.data()));

    find_physical_device(devices, usage, window ? window->surface : VK_NULL_HANDLE);

    if (physical == VK_NULL_HANDLE)
        throw std::runtime_error("No suitable physical device found.");

    init_logical_device(context, usage);

    if (physical_device_info.graphics_family_idx.has_value())
        vkGetDeviceQueue(logical, physical_device_info.graphics_family_idx.value(), 0, &graphics_queue);
    if (physical_device_info.present_family_idx.has_value())
        vkGetDeviceQueue(logical, physical_device_info.present_family_idx.value(), 0, &present_queue);
}

Device::~Device()
{
    vkDeviceWaitIdle(logical);
    vkDestroyDevice(logical, nullptr);
}

static void get_physical_device_info(PhysicalDeviceInfo& info, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    info.name = properties.deviceName;
    info.type = properties.deviceType;

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            info.heap_size = memoryProperties.memoryHeaps[memoryProperties.memoryTypes[i].heapIndex].size;
            break;
        }
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            info.graphics_family_idx = i;

        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support)
                info.present_family_idx = i;
        }
    }
}

static bool meets_extension_requirements(VkPhysicalDevice device, const std::vector<const char*>& required_extensions)
{
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> unmet_extensions(required_extensions.begin(), required_extensions.end());

    for (const auto& extension : available_extensions)
        unmet_extensions.erase(extension.extensionName);

    return unmet_extensions.empty();
}

static bool meets_swap_chain_requirements(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    if (surface == VK_NULL_HANDLE)
        return false;
    SwapChainSupport swap_chain_support;
    query_swap_chain_support(swap_chain_support, device, surface);
    return !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
}

static void get_required_extensions(std::vector<const char*>& required_extensions, DeviceUsage usage)
{
    if (usage & DEVICE_USAGE_WINDOW_BIT)
        required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

static int rank_device_type(VkPhysicalDeviceType type)
{
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return 0;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 1;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 2;
        default:
            return 0;
    }
}

// TODO: make more accurate comparison (perhaps a scoring system)
static int compare_physical_device(const PhysicalDeviceInfo& info1, const PhysicalDeviceInfo& info2)
{
    if (rank_device_type(info1.type) != rank_device_type(info2.type))
        return rank_device_type(info2.type) - rank_device_type(info1.type);
    return info2.heap_size - info1.heap_size;
}

void Device::find_physical_device(
    const std::vector<VkPhysicalDevice>& devices,
    DeviceUsage usage,
    VkSurfaceKHR surface)
{
    std::cout << "Found " << devices.size() << " physical devices:" << std::endl;

    std::vector<const char*> required_extensions;
    get_required_extensions(required_extensions, usage);

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    PhysicalDeviceInfo best_device_info;
    for (auto device : devices) {

        PhysicalDeviceInfo info;
        get_physical_device_info(info, device, surface);
        std::cout << "    " << info.name;
        if (info.type == VK_PHYSICAL_DEVICE_TYPE_CPU)
            std::cout << " (CPU)";
        std::cout << ", heap size: " << info.heap_size / (1024 * 1024) << " MB" << std::endl;

        bool is_suitable = meets_extension_requirements(device, required_extensions) &&
                           info.graphics_family_idx.has_value() &&
                           (!(usage & DEVICE_USAGE_WINDOW_BIT) ||
                            (meets_swap_chain_requirements(device, surface) && info.present_family_idx.has_value()));

        if (is_suitable && (best_device == VK_NULL_HANDLE || compare_physical_device(best_device_info, info) > 0)) {
            best_device = device;
            best_device_info = info;
        }
    }

    std::cout << "Using physical device " << best_device_info.name << "." << std::endl;

    physical = best_device;
    physical_device_info = best_device_info;
    query_swap_chain_support(swap_chain_support, best_device, surface);
}

void Device::init_logical_device(VulkanContext& context, DeviceUsage usage)
{
    std::vector<const char*> required_extensions;
    get_required_extensions(required_extensions, usage);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {
        physical_device_info.graphics_family_idx.value(),
        physical_device_info.present_family_idx.value()
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
    create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(context.validation_layers.size());
    create_info.ppEnabledLayerNames = context.validation_layers.data();

    if (vkCreateDevice(physical, &create_info, nullptr, &logical) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device.");
}

void Device::create_image(
    VkImage& image,
    VkDeviceMemory& memory,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = format;
    image_create_info.tiling = tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(logical, &image_create_info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image.");

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(logical, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_suitable_memory_type(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(logical, &alloc_info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory.");

    vkBindImageMemory(logical, image, memory, 0);
}

VkFormat Device::find_image_format(
    const std::vector<VkFormat>& desirable,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (VkFormat format : desirable) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("Failed to find format.");
}

uint32_t Device::find_suitable_memory_type(
    int32_t typeFilter,
    VkMemoryPropertyFlags desired_flags)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found.");
}

VkImageView Device::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask)
{
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = image;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = format;
    view_create_info.subresourceRange.aspectMask = aspect_mask;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(logical, &view_create_info, nullptr, &image_view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view.");

    return image_view;
}

void Device::create_buffer(
    VkBuffer& buffer,
    VkDeviceMemory& memory,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags mem_flags)
{
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(logical, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vertex buffer.");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(logical, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_suitable_memory_type(mem_requirements.memoryTypeBits, mem_flags);

    if (vkAllocateMemory(logical, &alloc_info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate memory for vertex buffer.");

    vkBindBufferMemory(logical, buffer, memory, 0);
}
