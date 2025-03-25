#include "scenetex.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

static VkFormat get_image_format_from_channel_count(int channels)
{
    // TODO deal with sRGB
    switch (channels) {
        case 1:
            return VK_FORMAT_R8_UNORM;
        case 2:
            return VK_FORMAT_R8G8_UNORM;
        case 3:
            return VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            throw std::runtime_error("Invalid number of channels.");
    }
}

static std::string full_path(const std::string& base_directory, const std::string path)
{
    std::string result = base_directory + path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

static void create_texture_images(
    Device& device,
    const std::string& base_directory,
    const std::vector<std::string>& texture_paths,
    std::vector<Texture>& textures,
    size_t& size,
    uint32_t& memory_type_bits)
{
    textures.resize(texture_paths.size());
    size = 0;
    memory_type_bits = UINT32_MAX;
    int width, height, channels;
    for (size_t i = 0; i < texture_paths.size(); i++) {
        std::string path = full_path(base_directory, texture_paths[i]);
        if (!stbi_info(path.c_str(), &width, &height, &channels))
            throw std::runtime_error("Failed to read image info at " + path);

        textures[i].format = VK_FORMAT_R8G8B8A8_UNORM; // TODO: get_image_format_from_channel_count(channels);
        textures[i].image = device.create_image(width, height, textures[i].format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        vkGetImageMemoryRequirements(device.logical_handle(), textures[i].image, &textures[i].memory_requirements);

        memory_type_bits &= textures[i].memory_requirements.memoryTypeBits;
        size = round_up_to(size + textures[i].memory_requirements.size, textures[i].memory_requirements.alignment);
    }
}

static void copy_buffer_to_image(VkCommandBuffer command_buffer, VkBuffer src, VkImage dst, VkExtent2D extent, VkImageLayout layout, VkDeviceSize buffer_offset)
{
    VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    transition_image_layout(command_buffer,
                            dst,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    VkBufferImageCopy region{};
    region.bufferOffset = buffer_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { extent.width, extent.height, 1 };

    vkCmdCopyBufferToImage(command_buffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transition_image_layout(command_buffer,
                            dst,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            layout,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            subresource_range);
}

static void load_texture_data(
    Device& device,
    CommandPool& command_pool,
    VkDeviceMemory memory,
    VkDeviceSize memory_size,
    const std::string& base_directory,
    const std::vector<std::string>& texture_paths,
    std::vector<Texture>& textures)
{
    VkCommandBuffer command_buffer = command_pool.begin_one_time_use_command_buffer();

    // TODO: perhaps it is possible to reduce the size of the staging buffer by not having all textures
    // in it at once. For large scenes, there may be problems due to having to fit two copies
    // of the texture data in memory.
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    device.create_buffer(staging_buffer,
                         staging_buffer_memory,
                         memory_size,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    uint8_t* mapped;
    vkMapMemory(device.logical_handle(), staging_buffer_memory, 0, memory_size, 0, (void**)&mapped);

    size_t offset = 0;
    int width, height, channels;
    for (size_t i = 0; i < texture_paths.size(); i++) {
        vkBindImageMemory(device.logical_handle(), textures[i].image, memory, offset);
        textures[i].view = device.create_image_view(textures[i].image, textures[i].format, VK_IMAGE_ASPECT_COLOR_BIT);

        std::string path = full_path(base_directory, texture_paths[i]);
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // TODO deal with images with fewer channels
        if (data == NULL)
            throw std::runtime_error("Failed to load image at " + path);

        size_t data_size = static_cast<size_t>(width) * height * 4;
        assert(data_size <= textures[i].memory_requirements.size);
        assert(offset + data_size <= memory_size);
        memcpy(mapped + offset, data, data_size);
        stbi_image_free(data);

        VkExtent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        copy_buffer_to_image(command_buffer, staging_buffer, textures[i].image, extent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, offset);

        offset = round_up_to(offset + textures[i].memory_requirements.size, textures[i].memory_requirements.alignment);
    }

    command_pool.end_one_time_use_command_buffer(command_buffer, device.get_graphics_queue());

    vkUnmapMemory(device.logical_handle(), staging_buffer_memory);
    vkDestroyBuffer(device.logical_handle(), staging_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), staging_buffer_memory, nullptr);
}

SceneTextures::SceneTextures(Device& device, CommandPool& command_pool, const Scene& scene, VkMemoryAllocateFlags allocate_flags)
    : device(device)
    , allocate_flags(allocate_flags)
{
    build(command_pool, scene);
}

SceneTextures::~SceneTextures()
{
    destroy();
}

void SceneTextures::rebuild(CommandPool& command_pool, const Scene& scene)
{
    destroy();
    build(command_pool, scene);
}

void SceneTextures::build(CommandPool& command_pool, const Scene& scene)
{
    if (!scene.get_texture_paths().empty()) {

        size_t size;
        uint32_t memory_type_flags;
        create_texture_images(device,
                              scene.get_resource_directory(),
                              scene.get_texture_paths(),
                              textures,
                              size,
                              memory_type_flags);

        if (memory_type_flags == 0)
            throw std::runtime_error("No common memory type was found for the scene's textures.");

        memory = device.allocate_memory(size, memory_type_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        load_texture_data(device,
                          command_pool,
                          memory,
                          size,
                          scene.get_resource_directory(),
                          scene.get_texture_paths(),
                          textures);
    }
}

void SceneTextures::destroy()
{
    for (const Texture& tex : textures) {
        vkDestroyImageView(device.logical_handle(), tex.view, nullptr);
        vkDestroyImage(device.logical_handle(), tex.image, nullptr);
    }
    if (!textures.empty())
        vkFreeMemory(device.logical_handle(), memory, nullptr);
}
