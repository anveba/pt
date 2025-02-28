#include "dispatch.h"
#include "display.h"
#include "fps.h"
#include "framechain.h"
#include "input.h"
#include "rasteriser.h"
#include "scene.h"
#include "ui.h"
#include "window.h"
#include <chrono>
#include <iostream>
#include <random>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Expected path to scene." << std::endl;
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Init error : " << SDL_GetError() << std::endl;
        return 1;
    }

    Camera camera(Vec3(0.0f, 0.0f, -10.0f),
                  Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::radians(45.0f),
                  1.0f,
                  0.1f,
                  1000.0f);
    Scene scene;
    scene.from_file(std::string(argv[1]), camera);

    CameraInput camera_input;

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VulkanContext context(ContextUsage(CONTEXT_USAGE_WINDOW_BIT), validation_layers);
    Window window(context, 800, 800);
    Device device(context, DeviceUsage(DEVICE_USAGE_WINDOW_BIT), &window);

    const SwapChainSupport& swap_chain_support = device.get_swap_chain_support();
    VkSurfaceFormatKHR surface_format = choose_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // choose_present_mode(swap_chain_support.present_modes);
    VkExtent2D extent = choose_extent(window, swap_chain_support.capabilities);
    VkFormat depth_format = choose_depth_format(device);

    Shader vs(device, "bin/rasteriser_vtx.spv");
    Shader ps(device, "bin/rasteriser_pxl.spv");
    Dispatcher dispatcher(device, DispatchUsage(DISPATCH_USAGE_RASTERISER_BIT | DISPATCH_USAGE_UI_BIT));
    Rasteriser rasteriser(device, dispatcher, vs, ps, extent, surface_format.format, depth_format);
    Display display(device, window, surface_format, depth_format, present_mode, extent);
    UserInterface ui(window, dispatcher, rasteriser);
    FramebufferChain framebuffers(display, &rasteriser);

    rasteriser.set_scene(dispatcher, scene);
    rasteriser.set_camera(dispatcher, camera);

    FpsCounter fps_counter(1.0f);
    fps_counter.restart();
    UiInfo ui_info;

    auto last_frame = std::chrono::high_resolution_clock::now();

    while (true) {
        auto this_frame = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(this_frame - last_frame).count();
        last_frame = this_frame;

        window.process_events(&camera_input);

        bool camera_rotated = camera_input.rotate(camera, 1.0f * delta_time);
        bool camera_moved = camera_input.move(camera, 4.0f * delta_time);
        if (camera_rotated || camera_moved)
            rasteriser.set_camera(dispatcher, camera);

        rasteriser.begin_render(&framebuffers);

        fps_counter.add_frame();

        ui_info.fps = fps_counter.frames_per_second();
        ui_info.cam_position = camera.position;
        ui_info.look_dir = glm::normalize(camera.rotation * Vec3(0.0f, 0.0f, 1.0f));
        ui_info.near = camera.near;
        ui_info.far = camera.far;
        ui.new_frame(ui_info);
        ui.render();

        VkSemaphore render_semaphore = rasteriser.end_render();
        display.present(render_semaphore);
    }
}