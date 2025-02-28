#include "ui.h"
#include "dispatch.h"
#include "rasteriser.h"
#include "window.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "User interface error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

std::vector<VkDescriptorPoolSize> UserInterface::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE } };
}

UserInterface::UserInterface(Window& window, Dispatcher& dispatcher, Rasteriser& rasteriser)
    : rasteriser(&rasteriser)
{
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(window.handle);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = rasteriser.device->context->instance;
    init_info.PhysicalDevice = rasteriser.device->physical;
    init_info.Device = rasteriser.device->logical;
    init_info.QueueFamily = rasteriser.device->physical_device_info.present_family_idx.value();
    init_info.Queue = rasteriser.device->present_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = dispatcher.descriptor_pool;
    init_info.RenderPass = rasteriser.render_pass;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
}
UserInterface::~UserInterface()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void UserInterface::new_frame(const UiInfo& info)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Debug Info");
    ImGui::Text("FPS: %.1f", info.fps);
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", info.cam_position.x, info.cam_position.y, info.cam_position.z);
    ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", info.look_dir.x, info.look_dir.y, info.look_dir.z);
    ImGui::Text("Clipping planes: %.2f, %.2f", info.near, info.far);
    ImGui::End();
}

void UserInterface::render()
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), rasteriser->command_buffer);
}