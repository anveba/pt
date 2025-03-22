#include "scenebuild.h"

SceneBuilder::SceneBuilder()
{
}

SceneBuilder::~SceneBuilder()
{
}

void SceneBuilder::set_material_scene(Scene& scene, Camera& camera)
{
    if (scene.object_variants.size() == 0)
        throw std::runtime_error("Scene has no objects.");

    ObjectVariant& sphere = scene.object_variants[0];

    constexpr size_t count = 10;
    constexpr size_t row_sz = 5;
    constexpr size_t lights = 2;
    constexpr float spacing = 6.0f;

    sphere.instances.resize(count + lights);
    scene.materials.resize(count + 1);

    for (size_t i = 0; i < count; i++) {
        scene.materials[i].clear();
        scene.materials[i].base_colour = Vec4(i / row_sz, 1 - i / row_sz, 0.0f, float(i % row_sz) / (row_sz - 1));
        scene.materials[i].emission = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        scene.materials[i].metalness_anisotropy = Vec4((i / row_sz) % 2, 0.0f, 0.0f, 0.0f);

        sphere.instances[i].transform = translation(Vec3(i % row_sz, i / row_sz, 0.0f) * spacing);
        sphere.instances[i].material_index = i;
    }

    scene.materials[count].clear();
    scene.materials[count].base_colour = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    scene.materials[count].emission = Vec4(1.0f, 1.0f, 1.0f, 15.0f);
    for (size_t i = count; i < count + lights; i++) {
        sphere.instances[i].transform = translation(Vec3(spacing * (i - count + 0.5f) * float(row_sz - 1) / lights, spacing * 2, spacing * 2));
        sphere.instances[i].material_index = count;
    }
}