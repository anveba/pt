#include "scenetex.h"

constexpr size_t MAX_TEXEL_BLOCK_SIZE = 16;

static void create_texture_images(
    Device& device,
    const std::string& base_directory,
    const std::vector<TextureData>& texture_data,
    std::vector<Texture>& textures,
    size_t& image_buffer_size,
    size_t& staging_buffer_size,
    uint32_t& memory_type_bits)
{
    textures.resize(texture_data.size());
    image_buffer_size = 0;
    staging_buffer_size = 0;
    memory_type_bits = UINT32_MAX;
    for (size_t i = 0; i < texture_data.size(); i++) {

        textures[i].format = texture_data[i].get_image_format();
        textures[i].image = device.create_image(
            texture_data[i].get_width(),
            texture_data[i].get_height(),
            textures[i].format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        vkGetImageMemoryRequirements(device.logical_handle(), textures[i].image, &textures[i].memory_requirements);
        textures[i].image_buffer_offset = image_buffer_size;

        memory_type_bits &= textures[i].memory_requirements.memoryTypeBits;
        image_buffer_size = round_up_to(image_buffer_size + textures[i].memory_requirements.size, textures[i].memory_requirements.alignment);
        staging_buffer_size += round_up_to(texture_data[i].get_data_size(), MAX_TEXEL_BLOCK_SIZE);
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
    VkDeviceSize staging_buffer_size,
    const std::string& base_directory,
    const std::vector<TextureData>& texture_data,
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
                         staging_buffer_size,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    uint8_t* mapped;
    vkMapMemory(device.logical_handle(), staging_buffer_memory, 0, staging_buffer_size, 0, (void**)&mapped);

    size_t offset = 0;
    for (size_t i = 0; i < texture_data.size(); i++) {
        vkBindImageMemory(device.logical_handle(), textures[i].image, memory, textures[i].image_buffer_offset);
        textures[i].view = device.create_image_view(textures[i].image, textures[i].format, VK_IMAGE_ASPECT_COLOR_BIT);

        assert(offset + texture_data[i].get_data_size() <= staging_buffer_size);
        assert(texture_data[i].get_data_size() <= textures[i].memory_requirements.size);
        memcpy(mapped + offset, texture_data[i].data_handle(), texture_data[i].get_data_size());

        VkExtent2D extent = { texture_data[i].get_width(), texture_data[i].get_height() };
        copy_buffer_to_image(command_buffer, staging_buffer, textures[i].image, extent, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, offset);

        offset += round_up_to(texture_data[i].get_data_size(), MAX_TEXEL_BLOCK_SIZE);
    }

    assert(offset == staging_buffer_size);

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
    if (!scene.get_textures().empty()) {

        size_t image_buffer_size, staging_buffer_size;
        uint32_t memory_type_flags;
        create_texture_images(device,
                              scene.get_resource_directory(),
                              scene.get_textures(),
                              textures,
                              image_buffer_size,
                              staging_buffer_size,
                              memory_type_flags);

        if (memory_type_flags == 0)
            throw std::runtime_error("No common memory type was found for the scene's textures.");

        memory = device.allocate_memory(image_buffer_size, memory_type_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        load_texture_data(device,
                          command_pool,
                          memory,
                          staging_buffer_size,
                          scene.get_resource_directory(),
                          scene.get_textures(),
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
