#include "dispatch.h"
#include "display.h"
#include "framechain.h"
#include "input.h"
#include "io/ioutil.h"
#include "io/obj_format.h"
#include "rasteriser.h"
#include "ui.h"
#include "window.h"
#include <iostream>
#include <random>

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Init error : " << SDL_GetError() << std::endl;
        return 1;
    }

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> scale_dist(0.1f, 2.0f);
    std::uniform_real_distribution<float> rot_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> translate_dist(-10.0f, 10.0f);

    ObjectVariant variant;
    variant.mesh = read_obj(str_from_file("data/bunny.obj"));
    variant.instances.resize(20);
    for (size_t i = 0; i < variant.instances.size(); i++) {
        Mat4 scale = scaling(Vec3(scale_dist(rng), scale_dist(rng), scale_dist(rng)));
        Mat4 rot = rotation(Vec3(rot_dist(rng), rot_dist(rng), rot_dist(rng)), rot_dist(rng) * 3.14f);
        Mat4 trans = translation(Vec3(translate_dist(rng), 0.0f, translate_dist(rng)));
        variant.instances[i].transform = Transform(trans * rot * scale);
    }
    Vec3 pos(0.0f, 0.0f, -10.0f);
    Camera camera(pos,
                  Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::radians(45.0f),
                  1.0f,
                  0.1f,
                  1000.0f);
    Scene scene({ variant }, {}, camera);

    CameraInput camera_input;

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VulkanContext context(CONTEXT_USAGE_WINDOW_BIT, validation_layers);
    Window window(context, 800, 800);
    Device device(context, DEVICE_USAGE_WINDOW_BIT, &window);

    const SwapChainSupport& swap_chain_support = device.get_swap_chain_support();
    VkSurfaceFormatKHR surface_format = choose_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = choose_present_mode(swap_chain_support.present_modes);
    VkExtent2D extent = choose_extent(window, swap_chain_support.capabilities);
    VkFormat depth_format = choose_depth_format(device);

    Shader vs(device, "bin/hello_vertex.spirv");
    Shader ps(device, "bin/hello_pixel.spirv");
    Dispatcher dispatcher(device, DispatchUsage(DISPATCH_USAGE_RASTERISER_BIT | DISPATCH_USAGE_UI_BIT));
    Rasteriser rasteriser(device, dispatcher, vs, ps, extent, surface_format.format, depth_format);
    Display display(device, window, surface_format, depth_format, present_mode, extent);
    UserInterface ui(window, dispatcher, rasteriser);
    FramebufferChain framebuffers(display, &rasteriser);

    rasteriser.set_scene(dispatcher, scene);

    while (true) {
        window.process_events(&camera_input);
        bool camera_rotated = camera_input.rotate(scene.camera(), 0.002f);
        bool camera_moved = camera_input.move(scene.camera(), 0.01f);
        if (camera_rotated || camera_moved) {
            rasteriser.set_camera(dispatcher, scene.camera());
        }

        rasteriser.begin_render(&framebuffers);
        ui.new_frame();
        ui.render();
        VkSemaphore render_semaphore = rasteriser.end_render();
        display.present(render_semaphore);
    }
}