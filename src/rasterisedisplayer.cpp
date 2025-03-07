#include "rasterisedisplayer.h"

#include "display.h"
#include "rasterise.h"

RasteriseDisplayer::RasteriseDisplayer(Display& display, Rasteriser& rasteriser)
    : depth_image(VK_NULL_HANDLE)
    , display(display)
    , rasteriser(rasteriser)
{
    create_image_semaphore();
    set_extent(display.get_extent().width, display.get_extent().height);
}

RasteriseDisplayer::~RasteriseDisplayer()
{
    vkDestroySemaphore(display.device.logical, image_semaphore, nullptr);
    destroy_framebuffers();
    destroy_depth_image();
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

void RasteriseDisplayer::set_scene(Dispatcher& dispatcher, const Scene& scene)
{
    rasteriser.set_scene(dispatcher, scene);
}

void RasteriseDisplayer::set_camera(Dispatcher& dispatcher, const Camera& camera)
{
    rasteriser.set_camera(dispatcher, camera);
}

void RasteriseDisplayer::wait_idle()
{
    rasteriser.wait_for_render();
}

void RasteriseDisplayer::begin_render()
{
    rasteriser.wait_for_render();
    VkFramebuffer framebuffer = framebuffers[display.acquire_next_index(image_semaphore)];
    rasteriser.begin_render(framebuffer);
}

void RasteriseDisplayer::end_render()
{
    VkSemaphore render_semaphore = rasteriser.end_render(&image_semaphore, 1);
    display.present(render_semaphore);
}

void RasteriseDisplayer::get_debug_info(RenderDebugInfo& info)
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
    return rasteriser.command_buffer;
}

void RasteriseDisplayer::create_framebuffers()
{
    framebuffers.resize(display.swap_chain.images.size());
    for (size_t i = 0; i < display.swap_chain.images.size(); i++) {

        const std::vector<VkImageView> attachments = { display.swap_chain.image_views[i], depth_image_view };

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = rasteriser.render_pass;
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_create_info.pAttachments = attachments.data();
        framebuffer_create_info.width = display.swap_chain.extent.width;
        framebuffer_create_info.height = display.swap_chain.extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(display.device.logical, &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

void RasteriseDisplayer::destroy_framebuffers()
{
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(display.device.logical, framebuffer, nullptr);
}

void RasteriseDisplayer::create_image_semaphore()
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(display.device.logical, &semaphore_create_info, nullptr, &image_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");
}

void RasteriseDisplayer::create_depth_image()
{
    display.device.create_image(depth_image,
                                depth_image_memory,
                                rasteriser.extent.width,
                                rasteriser.extent.height,
                                rasteriser.depth_format,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depth_image_view = display.device.create_image_view(depth_image, rasteriser.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void RasteriseDisplayer::destroy_depth_image()
{
    vkDestroyImageView(display.device.logical, depth_image_view, nullptr);
    vkDestroyImage(display.device.logical, depth_image, nullptr);
    vkFreeMemory(display.device.logical, depth_image_memory, nullptr);
}
