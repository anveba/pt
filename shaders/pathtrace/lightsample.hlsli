#ifndef LIGHTSAMPLE_HLSLI_INCLUDED
#define LIGHTSAMPLE_HLSLI_INCLUDED

#include "rng.hlsli"
#include "constants.h"

float alias_table_pmf(in StructuredBuffer<uint> buffer, uint index, uint bin_size, uint table_offset) {
    uint bin_offset = table_offset + index * bin_size;
    return asfloat(buffer[bin_offset + 0]);
}

// Returns the offset of the bin's value in the buffer and the probability.
uint sample_alias_table(
    in StructuredBuffer<uint> buffer, uint bin_count, uint bin_size, uint table_offset, inout Rng rng,
    out float p) 
{
    uint i = min(bin_count - 1, (uint)(next_float(rng) * bin_count));
    uint bin_offset = table_offset + i * bin_size;

    float q = asfloat(buffer[bin_offset + 1]);

    // Pick alias
    if (q < 1.0 && q < next_float(rng))  {
        i = buffer[bin_offset + 2];
        bin_offset = table_offset + i * bin_size;
    }
    
    p = asfloat(buffer[bin_offset + 0]);
    return bin_offset + 3;
}

float light_pdf(in StructuredBuffer<uint> light_sampler, uint emitter_index, uint primitive_index) {
    if (emitter_index == NO_EMITTER_INDEX)
        return 0.0;
    
    uint bin_offset = emitter_index * LIGHT_BIN_SIZE;
    float p = asfloat(light_sampler[bin_offset + 0]);
    uint light_type = light_sampler[bin_offset + 3];

    if (light_type == LIGHT_TYPE_EMITTER) {
        uint table_offset = light_sampler[bin_offset + 4];
        bin_offset = table_offset + primitive_index * EMITTER_BIN_SIZE;
        p *= asfloat(light_sampler[bin_offset + 0]);
    }
    return p;
}

float3 sample_light(
    in StructuredBuffer<uint> light_sampler, 
    in StructuredBuffer<float4> vertex_buffer,
    in StructuredBuffer<InstanceData> instance_buffer,
    in StructuredBuffer<PbrMaterial> material_buffer,
    in Texture2D<float4> textures[256], in SamplerState texture_sampler,
    uint light_count, inout Rng rng, in float3 pt,
    out float3 to_light, out float pdf) 
{
    uint bin_offset = sample_alias_table(light_sampler, light_count, LIGHT_BIN_SIZE, 0, rng, pdf);

    uint light_type = light_sampler[bin_offset + 0];

    if (light_type == LIGHT_TYPE_EMITTER) {

        uint table_offset = light_sampler[bin_offset + 1];
        uint tri_count = light_sampler[bin_offset + 2];
        uint instance_offset = light_sampler[bin_offset + 3];
        uint instance_count = light_sampler[bin_offset + 4];
        uint instance_id = instance_offset + min(instance_count - 1, (uint)(next_float(rng) * instance_count));

        float p;
        bin_offset = sample_alias_table(light_sampler, tri_count, EMITTER_BIN_SIZE, table_offset, rng, p);
        pdf *= p;

        InstanceData instance_data = get_instance_data(instance_buffer, instance_id);

        uint v_a_index = light_sampler[bin_offset + 0];
        uint v_b_index = light_sampler[bin_offset + 1];
        uint v_c_index = light_sampler[bin_offset + 2];

        Vertex v_a = get_vertex(vertex_buffer, instance_data.vertex_index + v_a_index);
        Vertex v_b = get_vertex(vertex_buffer, instance_data.vertex_index + v_b_index);
        Vertex v_c = get_vertex(vertex_buffer, instance_data.vertex_index + v_c_index);

        float3 p_a = (float3)mul(instance_data.transform, float4(v_a.position, 1.0));
        float3 p_b = (float3)mul(instance_data.transform, float4(v_b.position, 1.0));
        float3 p_c = (float3)mul(instance_data.transform, float4(v_c.position, 1.0));

        float3 bary = random_barycentric(rng);
        float3 point_on_tri = bary.x * p_a + bary.y * p_b + bary.z * p_c;
        to_light = point_on_tri - pt;

        float3 true_normal = cross(p_b - p_a, p_c - p_a);
        float area = sqrt(dot(true_normal, true_normal)) * 0.5;
        float cosine = abs(dot(normalize(to_light), normalize(true_normal)));
        float dist2 = dot(to_light, to_light);
        pdf *= dist2 / (area * cosine); 

        PbrMaterial material = get_material(material_buffer, instance_data.material_index);
        float2 uv = bary.x * v_a.uv + bary.y * v_b.uv + bary.z * v_c.uv;
        return get_emission(material, uv, textures, texture_sampler);

    } else if (light_type == LIGHT_TYPE_POINT) {

        float3 light_p = float3(
            asfloat(light_sampler[bin_offset + 1]),
            asfloat(light_sampler[bin_offset + 2]),
            asfloat(light_sampler[bin_offset + 3]));
        to_light = light_p - pt;
        return float3(
            asfloat(light_sampler[bin_offset + 4]),
            asfloat(light_sampler[bin_offset + 5]),
            asfloat(light_sampler[bin_offset + 6]));

    } else if (light_type == LIGHT_TYPE_DIRECTIONAL) {

        to_light = -float3(
            asfloat(light_sampler[bin_offset + 1]),
            asfloat(light_sampler[bin_offset + 2]),
            asfloat(light_sampler[bin_offset + 3]));
        return float3(
            asfloat(light_sampler[bin_offset + 4]),
            asfloat(light_sampler[bin_offset + 5]),
            asfloat(light_sampler[bin_offset + 6]));

    } else {
        // Should not be reached.
        to_light = 0.0;
        return 0.0;
    }
}

#endif