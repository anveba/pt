#include "rasterise.hlsli"

vs_out main(vs_in input, int id : SV_VertexID) {
  float4x4 instance_transform = float4x4(input.transform_col0, input.transform_col1, input.transform_col2, input.transform_col3);
  float3x3 instance_normal = float3x3(input.normal_col0, input.normal_col1, input.normal_col2);
  vs_out output = (vs_out)0; // zero the memory first
  output.position = mul(ubo.view_proj, mul(instance_transform, float4(input.position, 1.0)));
  output.world_position = input.position;
  output.normal = mul(instance_normal, input.normal);
  output.uv = input.uv;
  return output;
}