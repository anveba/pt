#ifndef GEOMETRY_HLSLI_INCLUDED
#define GEOMETRY_HLSLI_INCLUDED

struct InstanceData {
    uint vertex_index;
    uint index_index;
    uint emitter_index;
    uint material_index;
    row_major float4x4 transform;
    row_major float3x3 normal;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

InstanceData get_instance_data(in StructuredBuffer<InstanceData> instance_buffer, uint i) {
    return instance_buffer[i];
}

uint3 get_index(in StructuredBuffer<uint> index_buffer, uint i) {
    return uint3(index_buffer[i + 0], index_buffer[i + 1], index_buffer[i + 2]);
}

Vertex get_vertex(in StructuredBuffer<float4> vertex_buffer, uint i) {
    float4 v0 = vertex_buffer[i * 2 + 0];
    float4 v1 = vertex_buffer[i * 2 + 1];

    Vertex v;
    v.position = v0.xyz;
    v.normal = float3(v0.w, v1.xy);
    v.uv = v1.zw;
    return v;
}

void get_vertex_attributes(
    in float2 barycentric, in Vertex v_a, in Vertex v_b, in Vertex v_c, 
    out float3 normal, out float2 uv) 
{
    float alpha = 1.0f - barycentric.x - barycentric.y;
    float beta = barycentric.x;
    float gamma = barycentric.y;
    normal = normalize(alpha * v_a.normal + beta * v_b.normal + gamma * v_c.normal);
    uv = alpha * v_a.uv + beta * v_b.uv + gamma * v_c.uv;
}

void get_vertex_vectors(
    in Vertex v_a, in Vertex v_b, in Vertex v_c, 
    in float3 normal, in float2 uv, 
    out float3 tangent, out float3 bitangent, out float3 true_normal) 
{
    float2 duv_ab = v_b.uv - v_a.uv; 
    float2 duv_ac = v_c.uv - v_a.uv;
    float3 ab = v_b.position - v_a.position; 
    float3 ac = v_c.position - v_a.position;
    true_normal = cross(ab, ac);

    float denom = duv_ab.x * duv_ac.y - duv_ac.x * duv_ab.y;
    if (abs(denom) < 1e-5) {
        tangent = cross(normal, abs(normal.y) > 1e-5 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0));
    } else {
        tangent = (duv_ac.y * ab - duv_ab.y * ac) / denom;
        tangent = tangent - dot(tangent, normal) * normal;
    }

    bitangent = cross(normal, tangent);
}

#endif