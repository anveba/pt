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

float monodirectional_geometry_ggx(float cos_theta, float alpha) {
    float cos2_theta = cos_theta * cos_theta;
    float tan2_theta = (1.0 - cos2_theta) / cos2_theta;
    return 2.0 / (1.0 + sqrt(1.0 + alpha * alpha * tan2_theta));
}

float geometry_ggx_smith(float cos_ni, float cos_no, float alpha) {
    return monodirectional_geometry_ggx(cos_ni, alpha) * monodirectional_geometry_ggx(cos_no, alpha);
}

float distribution_ggx(float cos_nh, float alpha) {
    float alpha2 = alpha * alpha;
    float x = 1.0 + (alpha2 - 1.0) * cos_nh * cos_nh;
    return alpha2 / (PI * x * x);
}

float3 diffuse_fresnel(float3 f0, float cos_theta) {
    float base = 1.0 - cos_theta;
    return 1.0 + (f0 - 1.0) * base * base * base * base * base;
}

float3 sample_micronormal_ggx(float3 n, float3 x, float3 y, float alpha, uint4 rng_state) {


    float phi = 2.0 * PI * hybrid_taus(rng_state);
    float u = hybrid_taus(rng_state);
    float cos2_theta = (1.0 - u) / (1.0 + (alpha * alpha - 1.0) * u);
    float sin_theta = sqrt(1.0 - cos2_theta);
    float cos_theta = sqrt(cos2_theta);

    return n * cos_theta + x * sin_theta * cos(phi) + y * sin_theta * sin(phi);
}

[shader("closesthit")]
void main(inout RayPayload payload, in Attributes attributes)
{
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
        if (hybrid_taus(payload.rng_state) >= all_channels.a) {
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
    float material_alpha = roughness * roughness;
    material_alpha = lerp(0.01, 1.0, material_alpha);

    float3 o = normalize(-WorldRayDirection());
    float3 n = mul((float3x3)WorldToObject4x3(), obj_space_normal); // TODO investigate performance of WorldToObject4x3()
    if (dot(n, o) < 0.0)
        n = -n;
    float3 x = normalize(cross(abs(n.z) < 0.99 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0), n));
    float3 y = cross(n, x);
    // float3 h = sample_micronormal_ggx(n, material_alpha, payload.rng_state);
    // float3 i = 2.0 * dot(o, h) * h - o;
    float3 i = normalize(cosine_weighted_rand_dir(payload.rng_state, n));
    float3 h = normalize(i + o);

    float cos_ni = dot(n, i);
    float cos_no = dot(n, o);
    float cos_d = dot(h, i);
    float cos_nh = dot(n, h);

    float d = distribution_ggx(cos_nh, material_alpha);
    float3 f = fresnel_schlick(cos_d, f0);
    float g = geometry_ggx_smith(cos_ni, cos_no, material_alpha);

    float3 specular = (d * f * g) / (4.0 * cos_ni * cos_no);

    float f_d90 = 0.5 + 2.0 * roughness * cos_d * cos_d;
    float3 diffuse = (1.0 - metalness) * base_colour / PI * diffuse_fresnel(f_d90, cos_ni) * diffuse_fresnel(f_d90, cos_no);
    // float pdf = (d * cos_nh) / (4.0 * cos_d);
    float pdf = cos_ni / PI;
    payload.brdf = float4((specular + diffuse) * cos_ni, pdf);
    payload.emission = emission;
    payload.incoming_direction = float4(i, RayTCurrent());
}