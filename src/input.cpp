#include "input.h"

#include "camera.h"

CameraInput::CameraInput()
{
    up = right = down = left = false;
    north = east = south = west = ascend = descend = false;
}

void CameraInput::set_movement(SDL_Scancode code, bool value)
{
    switch (code) {
        case SDL_SCANCODE_UP:
            up = value;
            break;
        case SDL_SCANCODE_RIGHT:
            right = value;
            break;
        case SDL_SCANCODE_DOWN:
            down = value;
            break;
        case SDL_SCANCODE_LEFT:
            left = value;
            break;
        case SDL_SCANCODE_W:
            north = value;
            break;
        case SDL_SCANCODE_D:
            east = value;
            break;
        case SDL_SCANCODE_S:
            south = value;
            break;
        case SDL_SCANCODE_A:
            west = value;
            break;
        case SDL_SCANCODE_LSHIFT:
            ascend = value;
            break;
        case SDL_SCANCODE_SPACE:
            descend = value;
            break;
        default:
            break;
    }
}

void CameraInput::key_down(SDL_Scancode code)
{
    set_movement(code, true);
}

void CameraInput::key_up(SDL_Scancode code)
{
    set_movement(code, false);
}

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

bool CameraInput::rotate(Camera& camera, float step)
{
    float yaw = 0.0f, pitch = 0.0f;
    if (up)
        pitch += 1.0f;
    if (right)
        yaw += -1.0f;
    if (down)
        pitch += -1.0f;
    if (left)
        yaw += 1.0f;
    pitch *= step;
    yaw *= step;

    if (std::abs(pitch) < 1e-5 && std::abs(yaw) < 1e-5f)
        return false;

    const Vec3 up = camera.get_rotation() * Vec3(0.0f, 1.0f, 0.0f);
    const Vec3 forward = camera.get_rotation() * Vec3(0.0f, 0.0f, 1.0f);
    const Vec3 axis = glm::cross(up, forward);

    camera.get_rotation() = quaternion_from_rotation(up, yaw) *
                            quaternion_from_rotation(axis, pitch) *
                            camera.get_rotation();

    return true;
}

bool CameraInput::move(Camera& camera, float step)
{
    Vec3 movement(0.0f);
    if (north)
        movement += Vec3(0.0f, 0.0f, 1.0f);
    if (east)
        movement += Vec3(-1.0f, 0.0f, 0.0f);
    if (south)
        movement += Vec3(0.0f, 0.0f, -1.0f);
    if (west)
        movement += Vec3(1.0f, 0.0f, 0.0f);
    if (ascend)
        movement += Vec3(0.0f, 1.0f, 0.0f);
    if (descend)
        movement += Vec3(0.0f, -1.0f, 0.0f);
    movement *= step;
    movement = camera.get_rotation() * movement;

    if (glm::length(movement) < 1e-5)
        return false;

    camera.get_position() += movement;

    return true;
}