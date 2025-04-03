#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"
#include "material.hlsli"
#include "geometry.hlsli"

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

Vertex get_vertex(uint i) {
    float4 v0 = vertex_buffer[i * 2 + 0];
    float4 v1 = vertex_buffer[i * 2 + 1];

    Vertex v;
    v.position = v0.xyz;
    v.normal = float3(v0.w, v1.xy);
    v.uv = v1.zw;
    return v;
}

void get_emission(in PbrMaterial material, in float2 uv, out float3 emission) {
    if (map_bits(material) & EMISSION_MAP_BIT)
        emission = textures[asuint(material.emission.x)].SampleLevel(texture_sampler, uv, 0).rgb;
    else
        emission = material.emission.rgb;
    emission *= material.emission.a;
}

void get_base_colour(in PbrMaterial material, in float2 uv, out float4 base_colour) {
    if (map_bits(material) & BASE_COLOUR_MAP_BIT)
        base_colour = textures[asuint(material.base_colour.x)].SampleLevel(texture_sampler, uv, 0);
    else
        base_colour = material.base_colour;
}

void get_roughness_metalness_normal(
    in PbrMaterial material, in float2 uv, 
    out float roughness, out float metalness, out float3 normal) 
{
    if (map_bits(material) & ROUGHNESS_METALNESS_MAP_BIT) {
        float4 t = textures[asuint(material.rough_metal_normal_map_bits.x)].SampleLevel(texture_sampler, uv, 0);
        roughness = t.g;
        metalness = t.b;
    } else {
        if (map_bits(material) & ROUGHNESS_MAP_BIT)
            roughness = textures[asuint(material.rough_metal_normal_map_bits.x)].SampleLevel(texture_sampler, uv, 0).r;
        else
            roughness = material.rough_metal_normal_map_bits.x;

        if (map_bits(material) & METALNESS_MAP_BIT) 
            metalness = textures[asuint(material.rough_metal_normal_map_bits.y)].SampleLevel(texture_sampler, uv, 0).r;
        else
            metalness = material.rough_metal_normal_map_bits.y;
    }

    if (map_bits(material) & NORMAL_MAP_BIT)
        normal = normalize(textures[asuint(material.rough_metal_normal_map_bits.z)].SampleLevel(texture_sampler, uv, 0).xyz * 2.0 - 1.0);
    else
        normal = float3(0.0, 0.0, 1.0);
}

float3 fresnel_schlick(float cos_d, float3 f0) {
    float base = 1.0 - cos_d;
    return f0 + (1.0 - f0) * base * base * base * base * base;
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

void no_scatter(inout RayPayload payload) {
    payload.throughput = float4(0.0, 0.0, 0.0, INFINITY);
}

float diffuse_weight() {
    return 0.5; // TODO (also deal with metals)
}

float3 sample_direction(float3 o, float2 alpha, inout Rng rng, out float3 m, out float w, out float d, out float pdf) {
    float3 i;
    float p = diffuse_weight();
    bool sample_diffuse = next_float(rng) < p;
    if (sample_diffuse) {
        i = cosine_weighted_rand_dir(rng);
        m = normalize(o + i);
    } else {
        m = sample_visible_micronormal(o, alpha, rng);
        i = 2.0 * dot(o, m) * m - o;
    }

    d = distribution(m, alpha);
    float cos_d = dot(m, o);

    float pdf_m = (g1(o, alpha) / abs(cos_theta(o))) * d * abs(cos_d);
    float pdf_gtr = pdf_m / (4.0 * abs(cos_d)); //Due to the reflection transformation

    float pdf_cos = cos_theta(i) / PI;

    // https://cseweb.ucsd.edu/~viscomp/classes/cse168/sp21/readings/veach.pdf

    pdf = sample_diffuse ? (pdf_cos * p) : (pdf_gtr * (1.0 - p));
    float other_pdf = sample_diffuse ? (pdf_gtr * (1.0 - p)) : (pdf_cos * p) ;
    w = power_heuristic(pdf, other_pdf); // TODO compare heuristics

    return i;
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

    float3 obj_space_normal;
    float2 uv;
    get_vertex_attributes(attributes.barycentric, v_a, v_b, v_c, obj_space_normal, uv);

    get_emission(material, uv, payload.emission.rgb);
    if (is_final_bounce(payload)) 
        return;

    float4 base_colour;
    get_base_colour(material, uv, base_colour);

    // Check transparency
    if (next_float(payload.rng) >= base_colour.a) {
        payload.throughput = float4(1.0, 1.0, 1.0, RayTCurrent());
        payload.intersection = WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() + ORIGIN_OFFSET);
        payload.incoming_direction = WorldRayDirection();
        return;
    }

    float roughness, metalness;
    float3 mapped_normal;
    get_roughness_metalness_normal(material, uv, roughness, metalness, mapped_normal);

    float anisotropy = 0.0;

    float3 f0 = lerp(0.05, base_colour.rgb, metalness);
    roughness = max(0.00001, roughness * roughness);
    float aspect = sqrt(1.0 - 0.9 * abs(anisotropy));
    float2 material_alpha = float2(roughness / aspect, roughness * aspect); // Clamp?
    if (anisotropy < 0.0)
        material_alpha = float2(material_alpha.y, material_alpha.x);

    float3 obj_space_x, obj_space_y, true_obj_space_normal;
    get_vertex_vectors(v_a, v_b, v_c, obj_space_normal, uv, obj_space_x, obj_space_y, true_obj_space_normal);

    float3x3 obj_to_world = (float3x3)WorldToObject4x3(); // TODO look into performance of WorldToObject4x3()

    float3 world_x = mul(obj_to_world, obj_space_x);
    float3 world_y = mul(obj_to_world, obj_space_y);
    float3 world_normal = mul(obj_to_world, obj_space_normal);
    if (dot(world_normal, WorldRayDirection()) > 0.0) {
        world_x = -world_x;
        world_y = -world_y;
        world_normal = -world_normal;
    }

    float3 true_world_normal = mul(obj_to_world, true_obj_space_normal);
    if (dot(WorldRayDirection(), true_world_normal) > 0.0)
        true_world_normal = -true_world_normal;
    
    float3 mapped_x, mapped_y;
    onb(mapped_normal, mapped_x, mapped_y);

    float3x3 from_world_space = mul(float3x3(mapped_x, mapped_y, mapped_normal), float3x3(world_x, world_y, world_normal));
    float3x3 to_world_space = transpose(from_world_space);

    float3 o = normalize(mul(from_world_space, -WorldRayDirection()));
    if (o.z <= 0.0) { // May happen due to normal and true normal mismatch
        no_scatter(payload);
        return;
    }

    float3 m;
    float w, d, pdf;
    float3 i = sample_direction(o, material_alpha, payload.rng, m, w, d, pdf);

    if (i.z * o.z <= 0.0) {
        no_scatter(payload);
        return;
    }

    float cos_d = dot(m, o);

    float3 f = fresnel_schlick(cos_d, f0);
    float g = masking(o, i, material_alpha);

    float3 specular = (d * f * g) / (4.0 * cos_theta(i) * cos_theta(o));

    float f_d90 = 0.5 + 2.0 * roughness * cos_d * cos_d;
    float3 diffuse = (1.0 - metalness) * base_colour.rgb / PI * (1.0 - f);// * diffuse_fresnel(f_d90, cos_theta(i)) * diffuse_fresnel(f_d90, cos_theta(o));
    
    // float pdf = d * cos_theta(m);

    payload.throughput = float4(w * (specular + diffuse) * cos_theta(i) / pdf, RayTCurrent());
    payload.intersection = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + true_world_normal * ORIGIN_OFFSET;
    payload.incoming_direction = mul(to_world_space, i);
}