#include "device.h"

#include <cassert>
#include <set>

#include "pathtrace/rtext.h"

static void get_swap_chain_support(SwapChainSupport& support, VkPhysicalDevice device, VkSurfaceKHR surface)
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
    , context(context)
    , usage(usage)
{
    if (((usage & DEVICE_USAGE_WINDOW_BIT) == 0) != (window == nullptr))
        throw std::runtime_error("Window usage bit is not compatible with window parameter given.");

    uint32_t device_count;
    if (vkEnumeratePhysicalDevices(context.handle(), &device_count, nullptr) != VK_SUCCESS)
        throw std::runtime_error("Failed to enumerate physical devices.");
    if (device_count == 0)
        throw std::runtime_error("Could not find devices with Vulkan support.");

    std::vector<VkPhysicalDevice> devices(device_count);
    if (vkEnumeratePhysicalDevices(context.handle(), &device_count, devices.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to enumerate physical devices.");

    find_physical_device(devices, usage, window ? window->surface_handle() : VK_NULL_HANDLE);

    if (physical == VK_NULL_HANDLE)
        throw std::runtime_error("No suitable physical device found.");

    init_logical_device(context, usage);

    if (physical_device_info.graphics_family_idx.has_value())
        vkGetDeviceQueue(logical, physical_device_info.graphics_family_idx.value(), 0, &graphics_queue);
    if (physical_device_info.present_family_idx.has_value())
        vkGetDeviceQueue(logical, physical_device_info.present_family_idx.value(), 0, &present_queue);
    if (physical_device_info.graphics_family_idx.has_value()) // TODO split graphics and compute queue
        vkGetDeviceQueue(logical, physical_device_info.graphics_family_idx.value(), 0, &compute_queue);

    if (usage & DEVICE_USAGE_RAY_TRACE_BIT)
        load_ray_trace_functions(context.handle(), logical);
}

Device::~Device()
{
    vkDeviceWaitIdle(logical);
    vkDestroyDevice(logical, nullptr);
}

void Device::query_swap_chain_support(SwapChainSupport& support, const Window& window)
{
    get_swap_chain_support(support, physical, window.surface_handle());
}

static void query_physical_device_info(PhysicalDeviceInfo& info, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    vkGetPhysicalDeviceProperties(device, &info.properties);

    vkGetPhysicalDeviceFeatures(device, &info.features);

    // Get memory info
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if (memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            info.heap_size = memory_properties.memoryHeaps[memory_properties.memoryTypes[i].heapIndex].size;
            break;
        }
    }

    // Get ray tracing properties
    info.ray_tracing_properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    prop2.pNext = &info.ray_tracing_properties;
    vkGetPhysicalDeviceProperties2(device, &prop2);

    // Get ray tracing features
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &info.buffer_device_address_features;

    info.buffer_device_address_features = {};
    info.buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    info.buffer_device_address_features.pNext = &info.acceleration_structure_features;

    info.acceleration_structure_features = {};
    info.acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    info.acceleration_structure_features.pNext = &info.ray_tracing_pipeline_features;

    info.ray_tracing_pipeline_features = {};
    info.ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    info.ray_tracing_pipeline_features.pNext = &info.ray_query_features;

    info.ray_query_features = {};
    info.ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    vkGetPhysicalDeviceFeatures2(device, &features2);

    // Get queue info
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            info.graphics_family_idx = i; // TODO split compute and graphics queue
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
                info.present_family_idx = i;
            }
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

static bool meets_feature_requirements(const PhysicalDeviceInfo& device_info, DeviceUsage usage)
{
    if (usage & DEVICE_USAGE_RAY_TRACE_BIT) {
        return device_info.ray_query_features.rayQuery &&
               device_info.ray_tracing_pipeline_features.rayTracingPipeline &&
               device_info.buffer_device_address_features.bufferDeviceAddress &&
               device_info.acceleration_structure_features.accelerationStructure;
    }
    return true;
}

static bool meets_swap_chain_requirements(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    if (surface == VK_NULL_HANDLE)
        return false;
    SwapChainSupport swap_chain_support;
    get_swap_chain_support(swap_chain_support, device, surface);
    return !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
}

static void get_required_extensions(std::vector<const char*>& required_extensions, DeviceUsage usage)
{
    if (usage & DEVICE_USAGE_WINDOW_BIT) {
        required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (usage & DEVICE_USAGE_RAY_TRACE_BIT) {
        required_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        required_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        required_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        required_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        required_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }
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
    if (rank_device_type(info1.properties.deviceType) != rank_device_type(info2.properties.deviceType))
        return rank_device_type(info2.properties.deviceType) - rank_device_type(info1.properties.deviceType);
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
        query_physical_device_info(info, device, surface);
        std::cout << "    " << info.properties.deviceName;
        if (info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            std::cout << " (CPU)";
        std::cout << ", heap size: " << info.heap_size / (1024 * 1024) << " MB" << std::endl;

        bool is_suitable = info.graphics_family_idx.has_value() &&
                           meets_feature_requirements(info, usage) &&
                           meets_extension_requirements(device, required_extensions);

        if (usage & DEVICE_USAGE_WINDOW_BIT)
            is_suitable = is_suitable && info.present_family_idx.has_value() && meets_swap_chain_requirements(device, surface);

        if (is_suitable && (best_device == VK_NULL_HANDLE || compare_physical_device(best_device_info, info) > 0)) {
            best_device = device;
            best_device_info = info;
        }
    }
    if (best_device == VK_NULL_HANDLE)
        throw std::runtime_error("No suitable physical device was found.");

    std::cout << "Using physical device " << best_device_info.properties.deviceName << "." << std::endl;

    physical = best_device;
    physical_device_info = best_device_info;

    if (usage & DEVICE_USAGE_RAY_TRACE_BIT)
        std::cout << "Device has a maximum ray recursion depth of " << physical_device_info.ray_tracing_properties.maxRayRecursionDepth << "." << std::endl;
}

void Device::init_logical_device(VulkanContext& context, DeviceUsage usage)
{
    std::vector<const char*> required_extensions;
    get_required_extensions(required_extensions, usage);

    std::set<uint32_t> unique_queue_families = {
        physical_device_info.graphics_family_idx.value(),
        physical_device_info.present_family_idx.value()
    };
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    float queue_priority = 1.0f;
    for (uint32_t queue_family_idx : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_idx;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(context.get_validation_layers().size());
    create_info.ppEnabledLayerNames = context.get_validation_layers().data();

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    features2.features = {};
    
    create_info.pNext = &features2;
    create_info.pEnabledFeatures = nullptr;

    if (usage & DEVICE_USAGE_RAY_TRACE_BIT) {
        // Enable ray tracing features. Feature requirement test is assumed to have been passed.
        features2.pNext = &physical_device_info.buffer_device_address_features;
        physical_device_info.buffer_device_address_features.pNext = &physical_device_info.acceleration_structure_features;
        physical_device_info.acceleration_structure_features.pNext = &physical_device_info.ray_tracing_pipeline_features;
        physical_device_info.ray_tracing_pipeline_features.pNext = &physical_device_info.ray_query_features;
        physical_device_info.ray_query_features.pNext = nullptr;
    }

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

VkDeviceAddress Device::get_buffer_address(VkBuffer buffer)
{
    VkBufferDeviceAddressInfo address_info = {};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = buffer;

    return vkGetBufferDeviceAddress(logical, &address_info);
}

uint32_t Device::find_suitable_memory_type(
    int32_t typeFilter,
    VkMemoryPropertyFlags desired_flags)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags)
            return i;

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
    VkMemoryPropertyFlags mem_flags,
    VkMemoryAllocateFlags alloc_flags)
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

    VkMemoryAllocateFlagsInfo alloc_flags_info = {};
    alloc_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    alloc_flags_info.flags = alloc_flags;

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_suitable_memory_type(mem_requirements.memoryTypeBits, mem_flags);
    if (alloc_flags)
        alloc_info.pNext = &alloc_flags_info;

    if (vkAllocateMemory(logical, &alloc_info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate memory for vertex buffer.");

    vkBindBufferMemory(logical, buffer, memory, 0);
}
