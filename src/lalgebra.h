#ifndef LALGEBRA_H_INCLUDED
#define LALGEBRA_H_INCLUDED

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

typedef glm::mat3 Mat3;
typedef glm::mat4 Mat4;
typedef glm::mat4x3 Mat4x3;
typedef glm::mat3x4 Mat3x4;
typedef glm::vec2 Vec2;
typedef glm::vec3 Vec3;
typedef glm::vec4 Vec4;
typedef glm::quat Quaternion;
typedef glm::uvec4 Uint4;

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
Quaternion quaternion_from_rotation(Vec3 axis, float angle);

#endif