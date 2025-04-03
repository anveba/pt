#ifndef GEOMETRY_HLSLI_INCLUDED
#define GEOMETRY_HLSLI_INCLUDED

struct InstanceData {
    uint vertex_index;
    uint index_index;
    uint material_index;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

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

    if (duv_ab.x == 0.0 && duv_ab.y == 0.0 && duv_ac.x == 0.0 && duv_ac.y == 0.0) {
        duv_ab = float2(1.0, 0.0);
        duv_ac = float2(0.0, 1.0);
    }

    float3 ab = v_b.position - v_a.position; 
    float3 ac = v_c.position - v_a.position; 
    true_normal = normalize(cross(ab, ac));
    float denom = duv_ab.x * duv_ac.y - duv_ac.x * duv_ab.y;
    tangent = (duv_ac.y * ab - duv_ab.y * ac) / denom;
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    bitangent = cross(normal, tangent);
}

#endif