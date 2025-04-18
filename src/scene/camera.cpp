#include "camera.h"

Camera::Camera(Vec3 position,
               Quaternion rotation,
               float vertical_fov,
               float aspect_ratio,
               float near,
               float far)
    : position(position)
    , rotation(rotation)
    , vertical_fov(vertical_fov)
    , aspect_ratio(aspect_ratio)
    , near(near)
    , far(far)
    , focus_dist(0.0)
    , lens_radius(0.0)
{
}

Mat4 Camera::projection_matrix() const
{
    Mat4 proj = glm::perspective(vertical_fov, aspect_ratio, near, far);
    proj[1][1] *= -1.0f;
    return proj;
};