#ifndef LALGEBRA_H_INCLUDED
#define LALGEBRA_H_INCLUDED

#include <glm/glm.hpp>

typedef glm::mat3 Mat3;
typedef glm::mat4 Mat4;
typedef glm::vec2 Vec2;
typedef glm::vec3 Vec3;
typedef glm::vec4 Vec4;

Mat4 translation(Vec3 t);
Mat4 rotation(Vec3 axis, float angle);
Mat4 scaling(Vec3 s);
Mat4 projection(
    float near,
    float far,
    float left,
    float right,
    float top,
    float bottom);

#endif