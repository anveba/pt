#include "pathtrace.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<InstanceData> instance_buffer : register(t5);
StructuredBuffer<PbrMaterial> material_buffer : register(t6);
SamplerState texture_sampler : register(s7);
Texture2D<float4> textures[256] : register(t8);

PbrMaterial get_material(uint i) {
    return material_buffer[i];
}

InstanceData get_instance_data(uint i) {
    return instance_buffer[i];
}

uint3 get_index(uint i) {
    return uint3(index_buffer[i + 0], index_buffer[i + 1], index_buffer[i + 2]);
}

bool has_texture(uint i) {
    return i < 4294967295;
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

float3 fresnel_schlick(float cos_d, float3 f0) {
    float base = 1.0 - cos_d;
    return f0 + (1.0 - f0) * base * base * base * base * base;
}

float sq(float x) {
    return x * x;
}

float cos_theta(float3 v) {
    return v.z;
}

float cos2_theta(float3 v) {
    return sq(cos_theta(v));
}

float sin2_theta(float3 v) {
    return max(0.0, 1.0 - cos2_theta(v));
}

float sin_theta(float3 v) {
    return sqrt(sin2_theta(v));
}

float tan_theta(float3 v) {
    return sin_theta(v) / cos_theta(v);
}

float tan2_theta(float3 v) {
    return sin2_theta(v) / cos2_theta(v);
}

float lambda(float3 m, float2 alpha) {
    float tan2 = tan2_theta(m);
    if (isinf(tan2))
        return 0.0;

    float sin = sin_theta(m);
    float cos_phi = sin == 0.0 ? 1.0 : clamp(m.x / sin, -1.0, 1.0);
    float sin_phi = sin == 0.0 ? 0.0 : clamp(m.y / sin, -1.0, 1.0);
    float alpha2 = sq(cos_phi * alpha.x) + sq(sin_phi * alpha.y);
    return (sqrt(1.0 + alpha2 * tan2) - 1.0) / 2.0;
}

float g1(float3 v, float2 alpha) {
    return 1.0 / (1.0 + lambda(v, alpha));
}

float masking(float3 o, float3 i, float2 alpha) {
    return 1.0 / (1.0 + lambda(o, alpha) + lambda(i, alpha));
}

float distribution(float3 m, float2 alpha) {
    float tan2 = tan2_theta(m);
    if (isinf(tan2))
        return 0.0;

    float cos4 = sq(cos2_theta(m));
    float sin = sin_theta(m);
    float cos_phi = sin == 0.0 ? 1.0 : clamp(m.x / sin, -1.0, 1.0);
    float sin_phi = sin == 0.0 ? 0.0 : clamp(m.y / sin, -1.0, 1.0);
    float e = tan2 * (sq(cos_phi / alpha.x) + sq(sin_phi / alpha.y));
    return 1.0 / (PI * alpha.x * alpha.y * cos4 * sq(1.0 + e)); 
}

float3 diffuse_fresnel(float3 fd90, float cos_theta) {
    float base = 1.0 - cos_theta;
    return 1.0 + (fd90 - 1.0) * base * base * base * base * base;
}

// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void onb(const in float3 n, out float3 x, out float3 y) {
    const float s = n.z < 0 ? -1 : 1;
    const float a = -1.0f / (s + n.z);
    const float b = n.x * n.y * a;
    x = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    y = float3(b, s + n.y * n.y * a, -n.y);
}

// https://hal.science/hal-01509746/document
// Assumes v.z is positive
float3 sample_visible_micronormal(float3 v, float2 alpha, inout Rng rng) {

    v = normalize(float3(alpha.x * v.x, alpha.y * v.y, v.z));

    float3 x = (v.z < 0.99999) ? normalize(cross(float3(0.0, 0.0, 1.0), v)) : float3(1.0, 0.0, 0.0);
    float3 y = cross(v, x);

    float u1 = next_float(rng);
    float u2 = next_float(rng);

    float a = 1.0 / (1.0 + v.z);
    float r = sqrt(u1);
    float phi = (u2 < a) ? u2 / a * PI : PI + (u2 - a) / (1.0 - a) * PI;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi) * ((u2 < a) ? 1.0 : v.z);

    float3 m = p1 * x + p2 * y + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * v;
    return normalize(float3(alpha.x * m.x, alpha.y * m.y, max(1e-6, m.z)));

    // float2 p = uniform_sample_disk(rng_state);
    // p = 0;

    // float h = sqrt(1.0 - p.x * p.x);
    // p.y = lerp((1.0 + v.z) / 2.0, h, p.y);

    // float p_z = sqrt(max(0.0, 1.0 - dot(p, p)));
    // float3 nh = p.x * x + p.y * y + p_z * v;
    
    // return normalize(float3(alpha.x * nh.x, alpha.y * nh.y, max(1e-6, nh.z)));
}

float3 sample_micronormal(float3 v, float2 alpha, inout Rng rng) {

    float phi = 2.0 * PI * next_float(rng);
    float u = next_float(rng);
    float l = sqrt(u / (1.0 - u));
    float x = alpha.x * cos(phi) * l;
    float y = alpha.y * sin(phi) * l;

    return normalize(float3(x, y, 1));
}

void no_scatter(inout RayPayload payload, float3 emission) {
    payload.brdf = float4(0.0, 0.0, 0.0, 0.0);
    payload.emission = emission;
    payload.incoming_direction = float4(0.0, 0.0, 0.0, INFINITY);
}

[shader("closesthit")]
void main(inout RayPayload payload, in Attributes attributes)
{
    //TODO only check emission for final bounces

    InstanceData instance_data = get_instance_data(InstanceID());
    PbrMaterial material = get_material(instance_data.material_index);
    uint3 indices = get_index(instance_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(instance_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(instance_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(instance_data.vertex_index + indices.z);

    float bary_alpha = 1.0f - attributes.barycentric.x - attributes.barycentric.y;
    float bary_beta = attributes.barycentric.x;
    float bary_gamma = attributes.barycentric.y;
    float3 obj_space_normal = normalize(bary_alpha * v_a.normal + bary_beta * v_b.normal + bary_gamma * v_c.normal);
    float2 uv = bary_alpha * v_a.uv + bary_beta * v_b.uv + bary_gamma * v_c.uv;

    float3 base_colour, emission;
    float roughness, metalness;
    if (has_texture(material.col_emi_rgh_spec_maps.x)) {

        float4 all_channels = textures[material.col_emi_rgh_spec_maps.x].SampleLevel(texture_sampler, uv, 0);

        // Check transparency
        if (next_float(payload.rng) >= all_channels.a) {
            payload.brdf = 1.0;
            payload.emission = 0.0;
            payload.incoming_direction = float4(WorldRayDirection(), RayTCurrent());
            return;
        }

        base_colour = all_channels.rgb;
    }
    else
        base_colour = material.base_colour.rgb;

    if (has_texture(material.col_emi_rgh_spec_maps.y))
        emission = textures[material.col_emi_rgh_spec_maps.y].SampleLevel(texture_sampler, uv, 0).rgb * material.emission.a;
    else
        emission = material.emission.rgb * material.emission.a;

    if (has_texture(material.col_emi_rgh_spec_maps.z))
        roughness = textures[material.col_emi_rgh_spec_maps.z].SampleLevel(texture_sampler, uv, 0).r;
    else
        roughness = material.base_colour.a;

    if (has_texture(material.shn_clcoat_metal_norm_maps.z))
        metalness = textures[material.shn_clcoat_metal_norm_maps.z].SampleLevel(texture_sampler, uv, 0).r;
    else
        metalness = material.metalness_anisotropy.r;

    float3 f0 = lerp(0.05, base_colour, metalness);
    roughness = max(0.00001, roughness * roughness);
    float2 material_alpha = float2(roughness, roughness);

    float3 world_normal = mul((float3x3)WorldToObject4x3(), obj_space_normal); // TODO investigate performance of WorldToObject4x3()
    if (dot(world_normal, WorldRayDirection()) > 0.0)
        world_normal = -world_normal;
    
    float3 x, y;
    onb(world_normal, x, y);

    float3x3 from_world_space = float3x3(x, y, world_normal);
    float3x3 to_world_space = transpose(from_world_space);

    float3 o = normalize(mul(from_world_space, -WorldRayDirection()));
    if (o.z <= 0.0) { // May happen due to normal and true normal mismatch
        no_scatter(payload, emission);
        return;
    }

    float3 m = sample_visible_micronormal(o, material_alpha, payload.rng);
    float3 i = 2.0 * dot(o, m) * m - o;

    if (i.z * o.z <= 0.0) {
        no_scatter(payload, emission);
        return;
    }

    float cos_d = dot(m, o);

    float d = distribution(m, material_alpha);
    float3 f = fresnel_schlick(cos_d, f0);
    float g = masking(o, i, material_alpha);

    float3 specular = (d * f * g) / (4.0 * cos_theta(i) * cos_theta(o));

    float f_d90 = 0.5 + 2.0 * roughness * cos_d * cos_d;
    float3 diffuse = (1.0 - metalness) * base_colour / PI * diffuse_fresnel(f_d90, cos_theta(i)) * diffuse_fresnel(f_d90, cos_theta(o));
    
    // float pdf = d * cos_theta(m);
    float pdf = (g1(o, material_alpha) / abs(cos_theta(o))) * d * abs(cos_d);
    pdf = pdf / (4.0 * abs(cos_d)); //Due to the reflection transformation

    payload.brdf = float4((specular + diffuse) * cos_theta(i), pdf);
    payload.emission = emission;
    payload.incoming_direction = float4(mul(to_world_space, i), RayTCurrent());
}