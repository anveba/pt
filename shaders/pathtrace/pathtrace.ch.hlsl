#include "pathtrace.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<ObjectData> object_buffer : register(t5);

ObjectData get_object_data(uint i) {
    return object_buffer[i];
}

uint3 get_index(uint i) {
    return uint3(index_buffer[i + 0], index_buffer[i + 1], index_buffer[i + 2]);
}

Vertex get_vertex(uint i) {
    float4 v0 = vertex_buffer[i * 2 + 0];
    float4 v1 = vertex_buffer[i * 2 + 1];

    Vertex v;
    v.position = v0.xyz;
    v.normal = float3(v0.w, v1.xy);
    v.uv = v1.zw;
    return v;
}

[shader("closesthit")]
void main(inout RayPayload payload, in Attributes attributes)
{
    ObjectData object_data = get_object_data(InstanceID());
    uint3 indices = get_index(object_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(object_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(object_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(object_data.vertex_index + indices.z);

    float alpha = 1.0f - attributes.barycentric.x - attributes.barycentric.y;
    float beta = attributes.barycentric.x;
    float gamma = attributes.barycentric.y;

    float dist = RayTCurrent();
    float3 obj_space_normal = normalize(alpha * v_a.normal + beta * v_b.normal + gamma * v_c.normal);
    float3 normal = mul((float3x3)WorldToObject4x3(), obj_space_normal); // TODO investigate performance of WorldToObject4x3()

    payload.next_direction = normalize(rand_dir(payload.rng_state));
    if (dot(normal, payload.next_direction) < 0.0) {
        payload.next_direction = -payload.next_direction;
    }

    payload.incoming_colour = float4(float3(1.0, 1.0, 1.0) * dot(normal, payload.next_direction), dist);
}