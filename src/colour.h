#ifndef COLOUR_H_INCLUDED
#define COLOUR_H_INCLUDED

#include <ostream>

struct Colour
{
    Colour(float r, float g, float b)
        : r(r)
        , g(g)
        , b(b)
    {
    }

    Colour(float v)
        : r(v)
        , g(v)
        , b(v)
    {
    }

    Colour()
        : r(0.0f)
        , g(0.0f)
        , b(0.0f)
    {
    }

    float r, g, b;

    const float* const float_ptr() const { return (float*)this; }

    Colour& operator+=(const Colour& c);
    Colour& operator-=(const Colour& c);
    Colour& operator*=(const Colour& c);
    Colour& operator*=(float v);
    Colour& operator/=(float v);
    Colour pow(float v) const;

    // Maps uniformly each component to 0-255. Clamps if value is <0 or >1.
    static unsigned char to_byte(float val);
};

Colour operator+(const Colour& c1, const Colour& c2);
Colour operator-(const Colour& c1, const Colour& c2);
Colour operator*(const Colour& c1, const Colour& c2);
Colour operator*(const Colour& c, float v);
Colour operator*(float v, const Colour& c);
Colour operator/(const Colour& c, float v);

std::ostream& operator<<(std::ostream& out, const Colour& c);

#endif