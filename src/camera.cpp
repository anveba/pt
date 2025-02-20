#include "camera.h"

Camera::Camera(
    const Mat4& translation,
    const Mat4& rotation,
    const Mat4& projection)
    : translation(translation)
    , rotation(rotation)
    , projection(projection)
{
}

Mat4 Camera::world_to_ndc_matrix() const
{
    return projection * glm::inverse(translation * rotation);
}