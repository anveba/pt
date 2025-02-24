#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

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

    Vec3& get_position() { return position; }
    const Vec3& get_position() const { return position; }
    Quaternion& get_rotation() { return rotation; }
    const Quaternion& get_rotation() const { return rotation; }
    Mat4 view_matrix() const { return glm::inverse(glm::translate(Mat4(1.0f), -position) * glm::mat4_cast(rotation)); }
    Mat4 projection_matrix() const;

  private:
    Vec3 position;
    Quaternion rotation;
    float vertical_fov;
    float aspect_ratio;
    float near, far;
};

#endif