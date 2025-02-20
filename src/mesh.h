#ifndef MESH_H_INCLUDED
#define MESH_H_INCLUDED

#include <string>
#include <vector>

#include "lalgebra.h"
#include "util.h"

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    static VkVertexInputBindingDescription binding_description();
    static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions();
};

// A triangle struct that refers to its vertices through indices
struct IndexedTriangle
{
    uint32_t indices[3];
};

// A triangle struct that owns its vertices
struct OwnedTriangle
{
    Vertex vertices[3];
};

// A geometric mesh represented by a list of vertices and faces which store
// three indices that refer to the vertices, much like the obj format.
// It uses 0-indexing.
class Mesh
{
  public:
    Mesh() {}

    Mesh(const std::vector<Vertex>& vertices,
         const std::vector<IndexedTriangle>& triangles)
        : vertices(vertices)
        , tris(triangles)
    {
    }

    const std::vector<Vertex>& get_vertices() const { return vertices; }
    const std::vector<IndexedTriangle>& get_indexed_triangles() const { return tris; }

  private:
    std::vector<Vertex> vertices;
    std::vector<IndexedTriangle> tris;
};

#endif