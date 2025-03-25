#include "display/windowapp.h"
#include "pathtrace/ptdispatch.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scenebuild.h"
#include <SDL3/SDL.h>
#include <iostream>

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

    Camera camera(Vec3(0.0f, 0.0f, 0.0f),
                  Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::radians(45.0f),
                  1.0f,
                  0.1f,
                  10000.0f);
    Scene scene;
    SceneBuilder builder;
    builder.read_scene_description(scene, camera, argv[1]);

    uint32_t width = 400, height = 400;
    camera.aspect_ratio = float(width) / height;

    std::cout << scene.scene_details() << std::endl;

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    bool headless = true;

    if (headless) {
        PathTraceDispatcher dispatcher(scene, width, height, validation_layers);
        PathTraceParameters params{
            .camera = camera,
            .out_path = "data/test.png",
            .samples = 64,
            .max_bounces = 4,
        };
        dispatcher.start(params);
    } else {
        WindowedApplication application(width, height, validation_layers);
        application.begin(scene, camera);
    }
}