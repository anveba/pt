#include "rasterisedisplayer.h"

RasteriseDisplayer::RasteriseDisplayer(
    Display& display,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    VkExtent2D extent,
    VkFormat image_format,
    VkFormat depth_format)
    : depth_image(VK_NULL_HANDLE)
    , image_semaphore(display.get_device())
    , render_semaphore(display.get_device())
    , render_fence(display.get_device(), true)
    , display(display)
    , command_pool(command_pool)
    , rasteriser(display.get_device(), descriptor_pool, command_pool, scene, extent, image_format, depth_format)
    , in_render(false)
{
    set_extent(display.get_extent().width, display.get_extent().height);
    command_pool.create_command_buffers(&command_buffer, 1);
}

RasteriseDisplayer::~RasteriseDisplayer()
{
    destroy_framebuffers();
    destroy_depth_image();
    command_pool.destroy_command_buffers(&command_buffer, 1);
}

void RasteriseDisplayer::set_extent(uint32_t width, uint32_t height)
{
    rasteriser.set_extent(width, height);

    if (depth_image != VK_NULL_HANDLE)
        destroy_depth_image();

    create_depth_image();

    destroy_framebuffers();
    create_framebuffers();
}

void RasteriseDisplayer::set_scene(const Scene& scene)
{
    rasteriser.set_scene(command_pool, scene);
}

void RasteriseDisplayer::set_camera(const Camera& camera)
{
    rasteriser.set_camera(command_pool, camera);
}

void RasteriseDisplayer::wait_idle()
{
    render_fence.wait();
}

void RasteriseDisplayer::begin_render()
{
    if (in_render)
        throw std::runtime_error("Rasteriser is already rendering.");

    render_fence.wait();

    rasteriser.update_uniforms();

    bool swap_chain_recreated;
    uint32_t index = display.acquire_next_index(image_semaphore, swap_chain_recreated);
    if (swap_chain_recreated)
        set_extent(display.get_extent().width, display.get_extent().height);

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    rasteriser.write_command_buffer(command_buffer, framebuffers[index]); // TODO do not write once per frame

    in_render = true;
}

void RasteriseDisplayer::end_render()
{
    if (!in_render)
        throw std::runtime_error("Render ended before having begun.");

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_semaphore.handle();
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore.handle() };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    render_fence.reset();

    if (vkQueueSubmit(display.get_device().get_graphics_queue(), 1, &submit_info, render_fence.handle()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    in_render = false;

    display.present(render_semaphore);
}

void RasteriseDisplayer::get_debug_info(RenderDebugInfo& info) const
{
    info = {};
}

void RasteriseDisplayer::set_settings(const UiControlPanel& control_panel)
{
}

VkRenderPass RasteriseDisplayer::get_render_pass()
{
    return rasteriser.render_pass;
}

VkCommandBuffer RasteriseDisplayer::get_command_buffer()
{
    return command_buffer;
}

void RasteriseDisplayer::create_framebuffers()
{
    framebuffers.resize(display.get_swap_chain().images.size());
    for (size_t i = 0; i < display.get_swap_chain().images.size(); i++) {

        const std::vector<VkImageView> attachments = { display.get_swap_chain().image_views[i], depth_image_view };

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = rasteriser.render_pass;
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_create_info.pAttachments = attachments.data();
        framebuffer_create_info.width = display.get_swap_chain().extent.width;
        framebuffer_create_info.height = display.get_swap_chain().extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(display.get_device().logical_handle(), &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

void RasteriseDisplayer::destroy_framebuffers()
{
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(display.get_device().logical_handle(), framebuffer, nullptr);
}

void RasteriseDisplayer::create_depth_image()
{
    depth_image = display.get_device().create_image(rasteriser.extent.width,
                                                    rasteriser.extent.height,
                                                    rasteriser.depth_format,
                                                    VK_IMAGE_TILING_OPTIMAL,
                                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(display.get_device().logical_handle(), depth_image, &mem_requirements);
    depth_image_memory = display.get_device().allocate_memory(mem_requirements.size, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkBindImageMemory(display.get_device().logical_handle(), depth_image, depth_image_memory, 0);
    depth_image_view = display.get_device().create_image_view(depth_image, rasteriser.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void RasteriseDisplayer::destroy_depth_image()
{
    vkDestroyImageView(display.get_device().logical_handle(), depth_image_view, nullptr);
    vkDestroyImage(display.get_device().logical_handle(), depth_image, nullptr);
    vkFreeMemory(display.get_device().logical_handle(), depth_image_memory, nullptr);
}
