#include "rasterise.hlsli"

float4 main(vs_out input) : SV_TARGET {
    float3 normal = normalize(input.normal);

    float diffuse = max(dot(ubo.inv_light_dir_norm, normal), 0.0);
    
    float3 view_dir = normalize(ubo.view_pos - input.world_position);
    float3 halfway = normalize(ubo.inv_light_dir_norm - view_dir);
    float specular = pow(max(dot(normal, halfway), 0.0), 32.0);

    float3 d_colour = float3(0.4, 0.8, 0.7);
    float3 s_colour = float3(1.0, 1.0, 1.0);
    float3 colour = specular * 0.7 * s_colour + (0.5 * diffuse + 0.03) * d_colour;
    
    const float inv_gamma = 1.0 / 2.2;
    return float4(pow(colour, float3(inv_gamma, inv_gamma, inv_gamma)), 1.0);
}