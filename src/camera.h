#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include "lalgebra.h"

class Camera
{
  public:
    Camera(
        const Mat4& translation,
        const Mat4& rotation,
        const Mat4& projection);

    Mat4& translation_matrix() { return translation; }
    Vec3 position() const { return Vec3(translation[0][3], translation[1][3], translation[2][3]); }
    Mat4& rotation_matrix() { return rotation; }
    Mat4 view_matrix() const { return glm::inverse(translation * rotation); }
    Mat4& projection_matrix() { return projection; }
    const Mat4& projection_matrix() const { return projection; }

    Mat4 world_to_ndc_matrix() const;

  private:
    Mat4 translation;
    Mat4 rotation;
    Mat4 projection;
};

#endif