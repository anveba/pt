#include "pathtrace.hlsli"
#include "geometry.hlsli"
#include "material.hlsli"

StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<InstanceData> instance_buffer : register(t5);
StructuredBuffer<PbrMaterial> material_buffer : register(t6);
SamplerState texture_sampler : register(s7);
Texture2D<float4> textures[MAX_TEXTURE_COUNT] : register(t8);

[shader("anyhit")]
void main(inout ShadowRayPayload payload, in Attributes attributes)
{
    InstanceData instance_data = get_instance_data(instance_buffer, InstanceID());
    PbrMaterial material = get_material(material_buffer, instance_data.material_index);

    // This should never be true (non-transparent materials should have the opaque flag in the BVH,
    // thus never calling the any-hit shader).
    if (!is_transparent(material)) 
        AcceptHitAndEndSearch();

    uint3 indices = get_index(index_buffer, instance_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(vertex_buffer, instance_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(vertex_buffer, instance_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(vertex_buffer, instance_data.vertex_index + indices.z);

    float3 obj_space_normal;
    float2 uv;
    get_vertex_attributes(attributes.barycentric, v_a, v_b, v_c, obj_space_normal, uv);

    float4 base_colour = get_base_colour(material, uv, textures, texture_sampler);
    payload.throughput *= (1.0 - base_colour.a);

    if (payload.throughput < 1e-5)
        AcceptHitAndEndSearch(); // This call isn't strictly necessary.
    else
        IgnoreHit();
}
