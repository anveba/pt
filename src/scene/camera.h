#ifndef SCENE_CAMERA_H_INCLUDED
#define SCENE_CAMERA_H_INCLUDED

#include "lalgebra.h"

class Camera
{
  public:
    Camera(Vec3 position,
           Quaternion rotation,
           float vertical_fov,
           float aspect_ratio,
           float near,
           float far);

    Mat4 view_matrix() const { return glm::inverse(glm::translate(Mat4(1.0f), -position) * glm::mat4_cast(rotation)); }
    Mat4 projection_matrix() const;

    Vec3 position;
    Quaternion rotation;
    float vertical_fov;
    float aspect_ratio;
    float near, far;
};

#endif