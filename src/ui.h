#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

class Window;
class Rasteriser;

class UserInterface
{
  public:
    UserInterface(Window& window, Rasteriser& rasteriser);
    ~UserInterface();

    void new_frame();
    void render();

  private:
    Rasteriser* rasteriser;

    UserInterface(UserInterface const&) = delete;
    void operator=(UserInterface const&) = delete;
};

#endif
