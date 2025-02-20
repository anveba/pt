#include <stdexcept>

#include "ioutil.h"
#include "obj_format.h"
#include "parseutil.h"
#include "token_stream.h"

struct TriangleInfo
{
    uint32_t position_indices[3];
    uint32_t normal_indices[3];
};

// A struct that is passed around during the parsing process.
struct ObjParserState
{
    ObjParserState()
    {
        positions = std::vector<Vec3>();
        normals = std::vector<Vec3>();
        tris = std::vector<TriangleInfo>();
    }

    ObjParserState(ObjParserState const&) = delete;
    void operator=(ObjParserState const&) = delete;

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<TriangleInfo> tris;
};

// Assumes the given token stream has the three coordinates as the next tokens
static void parse_vertex_position(TokenStream& tokens, ObjParserState& state)
{
    if (tokens.size() - tokens.current_index() != 3)
        throw std::runtime_error("Vertex does not have three coordinates");

    state.positions.push_back(parse_vector(tokens));
}

// Assumes the given token stream has the three coordinates as the next tokens
static void parse_vertex_normal(TokenStream& tokens, ObjParserState& state)
{
    if (tokens.size() - tokens.current_index() != 3)
        throw std::runtime_error("Normal does not have three coordinates");

    state.normals.push_back(parse_vector(tokens));
}

// Assumes the given token stream has the three indices as the next tokens
static void parse_triangle(TokenStream& tokens, ObjParserState& state)
{
    if (tokens.size() - tokens.current_index() != 3)
        throw std::runtime_error("Face was not a triangle");

    bool no_normals = false;

    TriangleInfo t;
    for (int i = 0; i < 3; i++) {
        TokenStream vertex_data(tokens.next(), '/');

        // Note: Obj is 1-indexed and we want 0-indexing.
        t.position_indices[i] = std::stoul(vertex_data.next()) - 1;

        if (vertex_data.done()) {
            no_normals = true;
            continue;
        }

        // Note: Texture coordinates currently unsupported

        t.normal_indices[i] = std::stoul(vertex_data.next()) - 1;
    }

    // If no normals are attached, generate ones based on the cross product
    if (no_normals) {
        Vec3 generated_normal = glm::cross(
            state.positions[t.position_indices[1]] - state.positions[t.position_indices[0]],
            state.positions[t.position_indices[2]] - state.positions[t.position_indices[1]]);

        t.normal_indices[0] = t.normal_indices[1] = t.normal_indices[2] = state.normals.size();
        state.normals.push_back(generated_normal);
    }

    state.tris.push_back(t);
}

static void parse_line(const std::string& line,
                       ObjParserState& state)
{
    TokenStream tokens(line, ' ');

    if (tokens.done())
        return;
    const std::string& curr = tokens.next();

    if (curr == "v")
        parse_vertex_position(tokens, state);
    else if (curr == "vn")
        parse_vertex_normal(tokens, state);
    else if (curr == "f")
        parse_triangle(tokens, state);
    else
        throw std::runtime_error("Unrecognised token " + curr);
}

Mesh read_obj(const std::string& raw)
{
    std::string chars_to_remove = "\r\t";
    std::string filtered = raw;
    filter_string(filtered, chars_to_remove);

    TokenStream lines(filtered, '\n');

    ObjParserState state;

    while (!lines.done())
        parse_line(lines.next(), state);

    // Create a dummy normal if there are none
    if (state.normals.size() == 0)
        state.normals.push_back(Vec3(0.0f, 1.0f, 0.0f));

    // TODO: remove duplicates
    // TODO: add uv support
    std::vector<Vertex> vertices;
    std::vector<IndexedTriangle> triangles;
    for (TriangleInfo tri_info : state.tris) {
        IndexedTriangle idx_tri;
        for (int i = 0; i < 3; i++) {
            Vertex v;
            v.position = state.positions[tri_info.position_indices[i]];
            v.normal = state.normals[tri_info.normal_indices[i]];
            v.uv = Vec2(0.0f);
            idx_tri.indices[i] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(v);
        }
        triangles.push_back(idx_tri);
    }

    return Mesh(vertices, triangles);
}