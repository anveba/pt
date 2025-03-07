#include "dispatch.h"
#include "display.h"
#include "fps.h"
#include "input.h"
#include "rasterise.h"
#include "rasterisedisplayer.h"
#include "rtdisplayer.h"
#include "scene.h"
#include "ui.h"
#include "window.h"
#include <chrono>
#include <iostream>
#include <random>

void render_loop(IDisplayable** displayers, Camera& camera, Display& display, Dispatcher& dispatcher, UiControlPanel& control_panel)
{
    Window& window = display.get_window();
    IDisplayable* current_displayer = displayers[control_panel.render_type];

    UserInterface::init_vulkan(dispatcher, *current_displayer);
    UserInterface ui(control_panel);

    FpsCounter fps_counter(0.2f);
    fps_counter.restart();
    CameraInput camera_input;
    UiInfo ui_info;
    WindowEventInfo window_event_info = {};

    auto last_frame = std::chrono::high_resolution_clock::now();

    while (true) {
        auto this_frame = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(this_frame - last_frame).count();
        last_frame = this_frame;

        window.process_events(window_event_info, &camera_input);

        if (window_event_info.exit)
            break;

        bool update_camera = false;

        if (control_panel.render_type != current_displayer->render_type()) {
            current_displayer->wait_idle();
            current_displayer = displayers[control_panel.render_type];
            UserInterface::destroy_vulkan();
            UserInterface::init_vulkan(dispatcher, *current_displayer);
            current_displayer->set_extent(display.get_extent().width, display.get_extent().height);
            update_camera = true;
        }

        if (window_event_info.resize) {
            display.recreate_swap_chain();
            current_displayer->set_extent(display.get_extent().width, display.get_extent().height);
            camera.aspect_ratio = float(display.get_extent().width) / display.get_extent().height;
            update_camera = true;
        }

        bool camera_rotated = camera_input.rotate(camera, 1.0f * delta_time);
        bool camera_moved = camera_input.move(camera, 4.0f * delta_time);
        if (camera_rotated || camera_moved || update_camera)
            current_displayer->set_camera(dispatcher, camera);

        current_displayer->set_settings(control_panel);

        current_displayer->begin_render();

        fps_counter.add_frame();

        ui_info.fps = fps_counter.frames_per_second();
        ui_info.cam_position = camera.position;
        ui_info.look_dir = glm::normalize(camera.rotation * Vec3(0.0f, 0.0f, 1.0f));
        ui_info.near = camera.near;
        ui_info.far = camera.far;
        current_displayer->get_debug_info(ui_info.render_info);
        ui.new_frame(ui_info);
        ui.render();

        current_displayer->end_render();
    }
    current_displayer->wait_idle();
    UserInterface::destroy_vulkan();
}

void begin_windowed_application(const Scene& scene, Camera& camera, RenderType render_type, const std::vector<const char*>& validation_layers)
{
    VulkanContext context(ContextUsage(CONTEXT_USAGE_WINDOW_BIT), validation_layers);

    Window window(context, 400, 400);
    Device device(context, DeviceUsage(DEVICE_USAGE_WINDOW_BIT | DEVICE_USAGE_RAY_TRACE_BIT), &window);
    Dispatcher dispatcher(device, DispatchUsage(DISPATCH_USAGE_RAY_TRACE_BIT | DISPATCH_USAGE_RASTERISER_BIT | DISPATCH_USAGE_UI_BIT));
    UserInterface::init(window);

    SwapChainSupport swap_chain_support;
    device.query_swap_chain_support(swap_chain_support, window);
    VkSurfaceFormatKHR surface_format = choose_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // choose_present_mode(swap_chain_support.present_modes);
    Display display(device, window, surface_format, present_mode);
    camera.aspect_ratio = float(display.get_extent().width) / display.get_extent().height;

    Shader vs(device, "bin/rasteriser_vtx.spv");
    Shader ps(device, "bin/rasteriser_pxl.spv");

    Shader ray_hit(device, "bin/raygen.spv");
    Shader ray_miss(device, "bin/miss.spv");
    Shader ray_closest_hit(device, "bin/closesthit.spv");

    VkFormat depth_format = choose_depth_format(device);
    Rasteriser rasteriser(device, dispatcher, vs, ps, display.get_extent(), surface_format.format, depth_format);
    RasteriseDisplayer rasterise_displayer(display, rasteriser);
    rasterise_displayer.set_scene(dispatcher, scene);
    rasterise_displayer.set_camera(dispatcher, camera);

    RayTracer ray_tracer(device, dispatcher, scene, ray_hit, ray_miss, ray_closest_hit, display.get_extent());
    RayTraceDisplayer ray_trace_displayer(display, ray_tracer);
    ray_trace_displayer.set_camera(dispatcher, camera);

    UiControlPanel control_panel{ .render_type = render_type,
                                  .max_bounces = ray_tracer.get_max_bounces(),
                                  .samples_per_frame = ray_tracer.get_samples_per_render() };

    IDisplayable* displayers[MAX_RENDER_TYPE]{ &ray_trace_displayer, &rasterise_displayer };

    render_loop(displayers, camera, display, dispatcher, control_panel);

    UserInterface::destroy();
}

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

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    RenderType render_type = RENDER_TYPE_RASTERISE;
    bool headless = false;

    if (headless) {

    } else {
        begin_windowed_application(scene, camera, render_type, validation_layers);
    }
}