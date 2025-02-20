#pragma once

#include "colour.h"
#include "token_stream.h"
#include "transform.h"

// Assumes the given token stream has the three components as the next tokens
Colour parse_colour(TokenStream& tokens);

// Assumes the given token stream has the three coordinates as the next tokens
Vec3 parse_vector(TokenStream& tokens);

// Assumes the given token stream has the three coordinates as the next tokens
Mat4 parse_translation(TokenStream& tokens);

// Assumes the given token stream has the three coordinates and angle
// as the next tokens
Mat4 parse_rotation(TokenStream& tokens);

// Assumes the given token stream has the three coordinates as the next tokens
Mat4 parse_scaling(TokenStream& tokens);
