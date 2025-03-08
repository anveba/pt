#ifndef SCENE_MESH_H_INCLUDED
#define SCENE_MESH_H_INCLUDED

#include <string>
#include <vector>

#include "lalgebra.h"
#include "util.h"

struct Vertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;

    static VkVertexInputBindingDescription binding_description(uint32_t binding);
    static std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions(uint32_t binding, uint32_t location_offset);
};

// A triangle struct that refers to its vertices through indices
struct IndexedTriangle
{
    uint32_t indices[3];
};

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

    friend class Scene;
};

#endif