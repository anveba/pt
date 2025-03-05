struct UniformBufferObject
{
	float4x4 mvp;
  float3 view_pos;
  float3 inv_light_dir_norm;
  float3x3 normal;
};

cbuffer ubo : register(b0, space0) { UniformBufferObject ubo; }

struct vs_in {
    [[vk::location(0)]] float3 position : LOCATION0;
    [[vk::location(1)]] float3 normal : LOCATION1;
    [[vk::location(2)]] float2 uv : LOCATION2;

    [[vk::location(3)]] float4 transform_col0 : LOCATION3;
    [[vk::location(4)]] float4 transform_col1 : LOCATION4;
    [[vk::location(5)]] float4 transform_col2 : LOCATION5;
    [[vk::location(6)]] float4 transform_col3 : LOCATION6;

    [[vk::location(7)]] float3 normal_col0 : LOCATION7;
    [[vk::location(8)]] float3 normal_col1 : LOCATION8;
    [[vk::location(9)]] float3 normal_col2 : LOCATION9;
};

struct vs_out {
    float4 position : SV_POSITION; // required output of VS
    float3 world_position : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

vs_out vs_main(vs_in input, int id : SV_VertexID) {
  float4x4 instance_transform = float4x4(input.transform_col0, input.transform_col1, input.transform_col2, input.transform_col3);
  float3x3 instance_normal = float3x3(input.normal_col0, input.normal_col1, input.normal_col2);
  vs_out output = (vs_out)0; // zero the memory first
  output.position = mul(ubo.mvp, mul(instance_transform, float4(input.position, 1.0)));
  output.world_position = input.position;
  output.normal = mul(ubo.normal, mul(instance_normal, input.normal));
  output.uv = input.uv;
  return output;
}

float4 ps_main(vs_out input) : SV_TARGET {
  float3 normal = normalize(input.normal);

  float diffuse = max(dot(ubo.inv_light_dir_norm, normal), 0.0);
  
  float3 view_dir = normalize(ubo.view_pos - input.world_position);
  float3 halfway = normalize(ubo.inv_light_dir_norm - view_dir);
  float specular = pow(max(dot(normal, halfway), 0.0), 32.0);

  float3 d_colour = float3(0.4, 0.8, 0.7);
  float3 s_colour = float3(1.0, 1.0, 1.0);
  float3 colour = specular * 0.7 * s_colour + (0.5 * diffuse + 0.03) * d_colour;
  float inv_gamma = 1 / 2.2;
  return float4(pow(colour, float3(inv_gamma, inv_gamma, inv_gamma)), 1.0);
}