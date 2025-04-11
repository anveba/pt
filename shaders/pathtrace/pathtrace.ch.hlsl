#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"
#include "material.hlsli"
#include "geometry.hlsli"
#include "lightsample.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<InstanceData> instance_buffer : register(t5);
StructuredBuffer<PbrMaterial> material_buffer : register(t6);
SamplerState texture_sampler : register(s7);
Texture2D<float4> textures[256] : register(t8);
StructuredBuffer<uint> light_sampler : register(t9);

struct BrdfEvalInput {
    float3 base_colour;
    float2 alpha;
    float metalness;
};

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

float get_diffuse_cdf(float metalness) {
    return lerp(0.5, 0.0, metalness); // TODO
}

float3 evaluate_brdf(in BrdfEvalInput input, float3 i, float3 o, float3 m, out float pdf) {

    float3 f0 = lerp(0.05, input.base_colour.rgb, input.metalness);
    float cos_d = dot(m, o);

    float3 f = fresnel_schlick(cos_d, f0);
    float g = masking(o, i, input.alpha);
    float d = distribution(m, input.alpha);

    float3 specular = (d * f * g) / (4.0 * cos_theta(i) * cos_theta(o));

    // float f_d90 = 0.5 + 2.0 * roughness * cos_d * cos_d;
    float3 diffuse = (1.0 - input.metalness) * input.base_colour.rgb / PI * (1.0 - f);// * diffuse_fresnel(f_d90, cos_theta(i)) * diffuse_fresnel(f_d90, cos_theta(o));

    const float diffuse_cdf = get_diffuse_cdf(input.metalness);
    float pdf_diffuse = cos_theta(i) / PI;
    float pdf_m = (g1(o, input.alpha) / abs(cos_theta(o))) * d * abs(cos_d);
    float pdf_gtr = pdf_m / (4.0 * abs(cos_d)); //Due to the reflection transformation
    pdf = pdf_diffuse * diffuse_cdf + pdf_gtr * (1.0 - diffuse_cdf);

    return specular + diffuse;
}

float3 sample_direction(float3 o, float2 alpha, float metalness, inout Rng rng, 
    out float3 m, out float w, out float pdf) {

    float3 i;

    const float diffuse_cdf = get_diffuse_cdf(metalness);

    float u = next_float(rng);
    if (u < diffuse_cdf) {
        i = cosine_weighted_rand_dir(rng);
        m = normalize(o + i);
    } else {
        m = sample_visible_micronormal(o, alpha, rng);
        i = 2.0 * dot(o, m) * m - o;
    }

    float pdf_diffuse = cos_theta(i) / PI;

    float cos_d = dot(m, o);

    float pdf_m = (g1(o, alpha) / abs(cos_theta(o))) * distribution(m, alpha) * abs(cos_d);
    float pdf_gtr = pdf_m / (4.0 * abs(cos_d)); //Due to the reflection transformation

    // https://cseweb.ucsd.edu/~viscomp/classes/cse168/sp21/readings/veach.pdf
    pdf_diffuse *= diffuse_cdf;
    pdf_gtr *= (1.0 - diffuse_cdf);
    pdf = (u < diffuse_cdf) ? pdf_diffuse : pdf_gtr;
    w = pdf / (pdf_diffuse + pdf_gtr); // TODO compare heuristics

    return i;
}

float3 light_contribution(
    in BrdfEvalInput brdf_input, float3 o, float3 intersection, in float3x3 from_world_space, inout Rng rng) 
{
    if (ubo.light_count == 0) 
        return 0.0;

    //TODO only calculate emission if the light is not occluded

    float pdf, light_dist;
    float3 light_dir;
    bool is_delta_light;
    float3 emission = sample_light(
        light_sampler, 
        vertex_buffer, 
        instance_buffer,
        material_buffer, 
        textures, 
        texture_sampler, 
        ubo.light_count, 
        rng, 
        intersection, 
        light_dir,
        light_dist, 
        pdf,
        is_delta_light);

    float3 i = mul(from_world_space, light_dir);
    if (i.z <= 0.0) 
        return 0.0;
    
    RayDesc ray_desc;
    ray_desc.Origin = intersection;
    ray_desc.Direction = light_dir;
    ray_desc.TMin = 0.0;
    ray_desc.TMax = light_dist - 1e-5;

    //TODO deal with opacity

    ShadowRayPayload payload;
    payload.is_occluded = true;
    TraceRay(
        bvh, 
        RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 
        0xff,
        0,
        0,
        1,
        ray_desc,
        payload);

    if (payload.is_occluded)
        return 0.0;

    i = normalize(i);
    float3 m = normalize(i + o);
    float pdf_brdf;
    float3 brdf = evaluate_brdf(brdf_input, i, o, m, pdf_brdf);

    if (is_delta_light) {
        return emission * brdf * cos_theta(i) / pdf;
    } else {
        float w = sq(pdf) / (sq(pdf) + sq(pdf_brdf));
        return w * emission * brdf * cos_theta(i) / pdf;
    }
}

[shader("closesthit")]
void main(inout RayPayload payload, in Attributes attributes)
{
    InstanceData instance_data = get_instance_data(instance_buffer, InstanceID());
    PbrMaterial material = get_material(material_buffer, instance_data.material_index);
    uint3 indices = get_index(index_buffer, instance_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(vertex_buffer, instance_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(vertex_buffer, instance_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(vertex_buffer, instance_data.vertex_index + indices.z);

    float3 obj_space_normal;
    float2 uv;
    get_vertex_attributes(attributes.barycentric, v_a, v_b, v_c, obj_space_normal, uv);

    // TODO Emission calculation might not need UVs which is wasted computation if it is the final bounce
    float3 emission = get_emission(material, uv, textures, texture_sampler);

    if (is_first_ray(payload)) {
        add_radiance(payload, emission);
    
    } else if (max(max(emission.r, emission.g), emission.b) > 0.0) {

        float3 world_ab = mul((float3x3)instance_data.transform, v_b.position - v_a.position);
        float3 world_ac = mul((float3x3)instance_data.transform, v_c.position - v_a.position);
        float3 emitter_scaled_world_normal = cross(world_ab, world_ac);

        float area = sqrt(dot(emitter_scaled_world_normal, emitter_scaled_world_normal)) * 0.5;
        float dist2 = RayTCurrent() * RayTCurrent();
        float cosine = abs(dot(WorldRayDirection(), normalize(emitter_scaled_world_normal)));

        float pdf_l = light_pdf(light_sampler, instance_data.emitter_index, PrimitiveIndex()); 
        pdf_l *= dist2 / (area * cosine);
        if (pdf_l > 0.0 && dist2 > 0.0) {
            float w = sq(get_brdf_pdf(payload)) / (sq(get_brdf_pdf(payload)) + sq(pdf_l));
            add_radiance(payload, w * emission);
        }
    } 

    if (is_final_segment(payload)) 
        return;

    float4 base_colour = get_base_colour(material, uv, textures, texture_sampler);

    // Check transparency
    if (base_colour.a < 1.0 && base_colour.a < next_float(payload.rng)) {
        float3 intersection = WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() + ORIGIN_OFFSET);
        set_next_ray(payload, intersection, WorldRayDirection());
        return;
    }

    BrdfEvalInput brdf_input;
    brdf_input.base_colour = base_colour.rgb;
    float roughness;
    float3 mapped_normal;
    get_roughness_metalness_normal(material, uv, textures, texture_sampler, roughness, brdf_input.metalness, mapped_normal);

    float anisotropy = 0.0;
    
    roughness = max(0.00001, roughness * roughness);
    float aspect = sqrt(1.0 - 0.9 * abs(anisotropy));
    brdf_input.alpha = float2(roughness / aspect, roughness * aspect); // Clamp?
    if (anisotropy < 0.0)
        brdf_input.alpha = float2(brdf_input.alpha.y, brdf_input.alpha.x);

    float3 obj_space_x, obj_space_y, true_obj_space_normal;
    get_vertex_vectors(v_a, v_b, v_c, obj_space_normal, uv, obj_space_x, obj_space_y, true_obj_space_normal);

    float3 world_x = normalize(mul(instance_data.normal, obj_space_x));
    float3 world_y = normalize(mul(instance_data.normal, obj_space_y));
    float3 world_normal = normalize(mul(instance_data.normal, obj_space_normal));
    if (dot(world_normal, WorldRayDirection()) > 0.0) {
        world_x = -world_x;
        world_y = -world_y;
        world_normal = -world_normal;
    }

    float3 true_world_normal = normalize(mul(instance_data.normal, true_obj_space_normal));
    if (dot(WorldRayDirection(), true_world_normal) > 0.0)
        true_world_normal = -true_world_normal;
    
    float3x3 from_world_space = float3x3(world_x, world_y, world_normal);
    if (mapped_normal.z < 0.999) {
        float3 mapped_x, mapped_y;
        onb(mapped_normal, mapped_x, mapped_y);
        from_world_space = mul(float3x3(mapped_x, mapped_y, mapped_normal), from_world_space);
    }

    float3x3 to_world_space = transpose(from_world_space);

    float3 o = normalize(mul(from_world_space, -WorldRayDirection()));

    // The view direction may be in the opposite hemisphere than the normal due to normal and true normal mismatch
    if (o.z <= 0.0) {
        no_scatter(payload);
        return;
    }

    float3 intersection = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + true_world_normal * ORIGIN_OFFSET;
    add_radiance(payload, light_contribution(brdf_input, o, intersection, from_world_space, payload.rng));

    float3 m;
    float w, pdf;
    float3 i = sample_direction(o, brdf_input.alpha, brdf_input.metalness, payload.rng, m, w, pdf);

    if (i.z * o.z <= 0.0) {
        no_scatter(payload);
        return;
    }
    // float pdf = d * cos_theta(m);

    float pdf_brdf;
    float3 brdf = evaluate_brdf(brdf_input, i, o, m, pdf_brdf);
    float3 throughput = w * brdf * cos_theta(i) / pdf;
    set_brdf_pdf(payload, pdf_brdf);

    accumulate_throughput(payload, throughput);
    set_next_ray(payload, intersection, normalize(mul(to_world_space, i)));
}