#include "lalgebra.h"

#include <stdexcept>

Mat4 translation(Vec3 t)
{
    Mat4 translation(1, 0, 0, t.x, 0, 1, 0, t.y, 0, 0, 1, t.z, 0, 0, 0, 1);
    return translation;
}

Mat4 rotation(Vec3 u, float angle)
{
    if (angle < 1e-5)
        return glm::mat4(1.0f);

    if (u.length() < 1e-5)
        throw std::runtime_error("Rotation axis was (close to) zero");

    u = glm::normalize(u);

    float cos = cosf(angle);
    float sin = sinf(angle);

    Mat4 rotation(
        // Row 1
        u.x * u.x + (1 - u.x * u.x) * cos,
        u.x * u.y * (1 - cos) - u.z * sin,
        u.x * u.z * (1 - cos) + u.y * sin,
        0,

        // Row 2
        u.y * u.x * (1 - cos) + u.z * sin,
        u.y * u.y + (1 - u.y * u.y) * cos,
        u.y * u.z * (1 - cos) - u.x * sin,
        0,

        // Row 3
        u.z * u.x * (1 - cos) - u.y * sin,
        u.z * u.y * (1 - cos) + u.x * sin,
        u.z * u.z + (1 - u.z * u.z) * cos,
        0,

        // Row 4
        0,
        0,
        0,
        1);

    return rotation;
}

Mat4 scaling(Vec3 s)
{
    Mat4 scaling(s.x, 0, 0, 0, 0, s.y, 0, 0, 0, 0, s.z, 0, 0, 0, 0, 1);

    return scaling;
}

Mat4 projection(
    float near,
    float far,
    float left,
    float right,
    float top,
    float bottom)
{
    float width = right - left;
    float height = top - bottom;
    float depth = far - near;

    float e = 1e-5f;
    if (fabsf(width) < e || fabsf(height) < e || fabsf(depth) < e)
        throw std::runtime_error("Invalid frustum");

    Mat4 projection(
        // Row 1
        2.0f * near / width,
        0.0f,
        (right + left) / width,
        0.0f,

        // Row 2
        0.0f,
        2.0f * near / height,
        (top + bottom) / height,
        0.0f,

        // Row 3
        0.0f,
        0.0f,
        -(far + near) / depth,
        -2.0f * far * near / depth,

        // Row 4
        0.0f,
        0.0f,
        -1.0f,
        0.0f);

    return projection;
}