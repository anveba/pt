#include "colour.h"
#include <cmath>

unsigned char Colour::to_byte(float val)
{
    if (val < 0.0)
        val = 0.0;
    else if (val > 1.0)
        val = 1.0;
    return (unsigned char)(val * 255.0);
}

Colour& Colour::operator+=(const Colour& c)
{
    *this = *this + c;
    return *this;
}

Colour& Colour::operator-=(const Colour& c)
{
    *this = *this - c;
    return *this;
}

Colour& Colour::operator*=(const Colour& c)
{
    *this = *this * c;
    return *this;
}

Colour& Colour::operator*=(float v)
{
    *this = *this * v;
    return *this;
}

Colour& Colour::operator/=(float v)
{
    *this = *this / v;
    return *this;
}

Colour Colour::pow(float v) const
{
    return Colour(
        powf(r, v),
        powf(g, v),
        powf(b, v));
}

Colour operator+(const Colour& c1, const Colour& c2)
{
    return Colour(c1.r + c2.r, c1.g + c2.g, c1.b + c2.b);
}

Colour operator-(const Colour& c1, const Colour& c2)
{
    return Colour(c1.r - c2.r, c1.g - c2.g, c1.b - c2.b);
}

Colour operator*(const Colour& c1, const Colour& c2)
{
    return Colour(c1.r * c2.r, c1.g * c2.g, c1.b * c2.b);
}

Colour operator*(const Colour& c, float v)
{
    return Colour(c.r * v, c.g * v, c.b * v);
}

Colour operator*(float v, const Colour& c)
{
    return c * v;
}

Colour operator/(const Colour& c, float v)
{
    return (1.0f / v) * c;
}

std::ostream& operator<<(std::ostream& out, const Colour& c)
{
    return out << c.r << ' ' << c.g << ' ' << c.b;
}