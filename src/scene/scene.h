#ifndef SCENE_SCENE_H_INCLUDED
#define SCENE_SCENE_H_INCLUDED

#include <vector>

#include "camera.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "transform.h"

struct Instance
{
    Transform transform;
    uint32_t material_index;
};

struct ObjectVariant
{
    std::vector<Instance> instances;
    Mesh mesh;
};

class Scene
{
  public:
    Scene()
    {
    }

    inline const std::vector<ObjectVariant>& get_object_variants() const { return object_variants; }
    inline const std::vector<PbrMaterial>& get_materials() const { return materials; }
    inline const std::vector<PointLight>& get_point_lights() const { return point_lights; }
    inline const std::vector<std::string>& get_texture_paths() const { return texture_paths; }
    inline const std::string& get_resource_directory() const { return resource_directory; }

    void from_file(const std::string& path, Camera& camera);
    std::string scene_details();

  private:
    std::vector<ObjectVariant> object_variants;
    std::vector<PointLight> point_lights;
    std::vector<PbrMaterial> materials;
    std::vector<std::string> texture_paths;

    std::string resource_directory;

    void clear();
    bool check_valid();

    friend class SceneBuilder;
};

#endif