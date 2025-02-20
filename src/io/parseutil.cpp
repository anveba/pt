#include <iostream>

#include "parseutil.h"
#include "token_stream.h"

Colour parse_colour(TokenStream& tokens)
{
    if (tokens.size() - tokens.current_index() < 3)
        throw std::runtime_error("Colours need three components");

    Colour col;
    col.r = std::stof(tokens.next());
    col.g = std::stof(tokens.next());
    col.b = std::stof(tokens.next());
    return col;
}

Vec3 parse_vector(TokenStream& tokens)
{
    if (tokens.size() - tokens.current_index() < 3)
        throw std::runtime_error("Vectors need three components");

    Vec3 pos;
    pos.x = std::stof(tokens.next());
    pos.y = std::stof(tokens.next());
    pos.z = std::stof(tokens.next());
    return pos;
}

Mat4 parse_translation(TokenStream& tokens)
{
    if (tokens.size() - tokens.current_index() != 3)
        throw std::runtime_error("Translation did not have three coordinates");

    float x = std::stof(tokens.next());
    float y = std::stof(tokens.next());
    float z = std::stof(tokens.next());

    return translation(Vec3(x, y, z));
}

Mat4 parse_rotation(TokenStream& tokens)
{
    if (tokens.size() - tokens.current_index() != 4)
        throw std::runtime_error(
            "Rotation did not have three coordinates and angle");

    Vec3 u;
    {
        float x = std::stof(tokens.next());
        float y = std::stof(tokens.next());
        float z = std::stof(tokens.next());
        u = Vec3(x, y, z);
    }

    float angle = std::stof(tokens.next());

    return rotation(u, angle);
}

Mat4 parse_scaling(TokenStream& tokens)
{
    if (tokens.size() - tokens.current_index() != 3)
        throw std::runtime_error("Scaling did not have three coordinates");

    float x = std::stof(tokens.next());
    float y = std::stof(tokens.next());
    float z = std::stof(tokens.next());

    return scaling(Vec3(x, y, z));
}