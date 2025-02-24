#ifndef SCENE_H_INCLUDED
#define SCENE_H_INCLUDED

#include <vector>

#include "camera.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "transform.h"

struct Instance
{
    Transform transform;
};

struct ObjectVariant
{
    std::vector<Instance> instances;
    Mesh mesh;
};

class Scene
{
  public:
    Scene(const std::vector<ObjectVariant>& object_variants,
          const std::vector<PointLight>& point_lights,
          const Camera& camera)
        : object_variants(object_variants)
        , point_lights(point_lights)
        , cam(camera)
        , transform(Mat4(1.0f))
    {
    }

    std::vector<ObjectVariant>& get_object_variants() { return object_variants; }
    const std::vector<ObjectVariant>& get_object_variants() const { return object_variants; }
    const std::vector<PointLight>& get_point_lights() const { return point_lights; }
    Camera& camera() { return cam; }
    const Camera& camera() const { return cam; }
    Transform& global_transform() { return transform; }
    const Transform& global_transform() const { return transform; }

  private:
    std::vector<ObjectVariant> object_variants;

    std::vector<PointLight> point_lights;

    Camera cam;

    Transform transform;
};

#endif