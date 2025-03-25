#ifndef IO_PARSEUTIL_H_INCLUDED
#define IO_PARSEUTIL_H_INCLUDED

#include "colour.h"
#include "scene/transform.h"
#include "tokenstream.h"

// Assumes the given token stream has the three coordinates as the next tokens
Vec3 parse_vector(TokenStream& tokens);

// Assumes the given token stream has the three coordinates as the next tokens
Mat4 parse_translation(TokenStream& tokens);

// Assumes the given token stream has the three coordinates and angle
// as the next tokens
Mat4 parse_rotation(TokenStream& tokens);

// Assumes the given token stream has the three coordinates as the next tokens
Mat4 parse_scaling(TokenStream& tokens);

#endif