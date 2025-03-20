#ifndef SCENE_SCENE_H_INCLUDED
#define SCENE_SCENE_H_INCLUDED

#include <vector>

#include "camera.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "transform.h"
#include "texturedata.h"

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

  const std::vector<ObjectVariant>& get_object_variants() const { return object_variants; }
  const std::vector<PbrMaterial>& get_materials() const { return materials; }
  const std::vector<PointLight>& get_point_lights() const { return point_lights; }
  const std::vector<std::string>& get_texture_paths() const { return texture_paths; }

  void from_file(const std::string &path, Camera &camera);

private:
  std::vector<ObjectVariant> object_variants;
  std::vector<PointLight> point_lights;
  std::vector<PbrMaterial> materials;
  std::vector<std::string> texture_paths;

  void clear();

  friend class SceneBuilder;

  NO_COPY(Scene);
};

#endif