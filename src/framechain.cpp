#include "framechain.h"

FramebufferChain::FramebufferChain(Display& display, IRenderer* renderer)
    : display(&display)
{
    if (renderer == nullptr)
        throw std::runtime_error("Renderer was null.");

    std::vector<VkImageView> extra_attachments = renderer->get_extra_attachments();

    framebuffers.resize(display.swap_chain.images.size());
    for (size_t i = 0; i < display.swap_chain.images.size(); i++) {

        std::vector<VkImageView> attachments = { display.swap_chain.image_views[i] };
        attachments.insert(attachments.end(), extra_attachments.begin(), extra_attachments.end());

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = renderer->get_render_pass();
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_create_info.pAttachments = attachments.data();
        framebuffer_create_info.width = display.swap_chain.extent.width;
        framebuffer_create_info.height = display.swap_chain.extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(display.device->logical, &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");

        attachments.pop_back();
    }
}

FramebufferChain::~FramebufferChain()
{
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(display->device->logical, framebuffer, nullptr);
}

VkFramebuffer FramebufferChain::acquire(VkSemaphore image_ready)
{
    return framebuffers[display->acquire_next_index(image_ready)];
}