#ifndef DISPLAY_INPUT_H_INCLUDED
#define DISPLAY_INPUT_H_INCLUDED

#include <SDL3/SDL.h>
class Camera;

class IInputHandler
{
  public:
    virtual void key_down(SDL_Scancode code) = 0;
    virtual void key_up(SDL_Scancode code) = 0;
};

class CameraInput : public IInputHandler
{
  public:
    CameraInput();

    virtual void key_down(SDL_Scancode code) override;
    virtual void key_up(SDL_Scancode code) override;

    bool rotate(Camera& camera, float step);
    bool move(Camera& camera, float step);

  private:
    bool up, right, down, left;
    bool north, west, south, east, ascend, descend;

    void set_movement(SDL_Scancode code, bool value);
};

#endif