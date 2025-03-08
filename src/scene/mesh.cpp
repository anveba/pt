#include "mesh.h"

VkVertexInputBindingDescription Vertex::binding_description(uint32_t binding)
{
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = binding;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding_description;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::attribute_descriptions(uint32_t binding, uint32_t location_offset)
{
    std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions;
    attribute_descriptions[0].binding = binding;
    attribute_descriptions[0].location = location_offset + 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);
    attribute_descriptions[1].binding = binding;
    attribute_descriptions[1].location = location_offset + 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, normal);
    attribute_descriptions[2].binding = binding;
    attribute_descriptions[2].location = location_offset + 2;
    attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[2].offset = offsetof(Vertex, uv);
    return attribute_descriptions;
}