#ifndef SCENE_MATERIAL_H_INCLUDED
#define SCENE_MATERIAL_H_INCLUDED

#include "colour.h"

class PhongMaterial
{
  public:
    PhongMaterial()
        : amb(Colour(1.0f, 0.0f, 1.0f))
        , dif(Colour(1.0f, 0.0f, 1.0f))
        , spe(Colour(1.0f, 0.0f, 1.0f))
        , shi(1.0)
    {
    }

    PhongMaterial(
        const Colour& ambient,
        const Colour& diffuse,
        const Colour& specular,
        float shininess)
        : amb(ambient)
        , dif(diffuse)
        , spe(specular)
        , shi(shininess)
    {
    }

    Colour& ambient() { return amb; }
    Colour& diffuse() { return dif; }
    Colour& specular() { return spe; }
    float& shininess() { return shi; }
    const Colour& ambient() const { return amb; }
    const Colour& diffuse() const { return dif; }
    const Colour& specular() const { return spe; }
    const float& shininess() const { return shi; }

  private:
    Colour amb, dif, spe;
    float shi;
};

#endif