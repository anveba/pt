struct UBO
{
	float4x4 mvp;
  float4x4 normal;
  float3 view_pos;
  float3 light_dir_view_space_norm;
};

cbuffer ubo : register(b0, space0) { UBO ubo; }

/* vertex attributes go here to input to the vertex shader */
struct vs_in {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv: UV;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct vs_out {
    float4 position : SV_POSITION; // required output of VS
    float3 normal : NORMAL0;
    float2 uv : UV;
};

vs_out vs_main(vs_in input, int id : SV_VertexID) {
  vs_out output = (vs_out)0; // zero the memory first
  output.position = mul(ubo.mvp, float4(input.position, 1.0));
  output.normal = mul((float3x3)ubo.normal, input.normal);
  output.uv = input.uv;
  return output;
}

float4 ps_main(vs_out input) : SV_TARGET {
  float3 light_dir = ubo.light_dir_view_space_norm;
  float3 normal = normalize(input.normal);

  float diffuse = abs(dot(light_dir, normal));
  
  float3 view_dir = normalize(ubo.view_pos - (float3)input.position);
  float3 reflection = reflect(-light_dir, normal);
  float3 halfway = normalize(light_dir + view_dir);
  float specular = pow(abs(dot(normal, halfway)), 64.0);

  float3 d_colour = float3(0.2, 1.0, 0.7);
  float3 s_colour = float3(1.0, 1.0, 1.0);
  float3 colour = specular * 0.7 * s_colour + (0.5 * diffuse + 0.03) * d_colour;
  float gamma = 2.2;
  return float4(pow(colour, float3(gamma, gamma, gamma)), 1.0); // must return an RGBA colour
}