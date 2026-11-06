cbuffer ObjectConstants : register(b0) { float4x4 model; };
cbuffer ShadowConstants : register(b1) { float4x4 lightViewProj; };

float4 main(float3 position : POSITION) : SV_Position
{
    return mul(mul(float4(position, 1.0f), model), lightViewProj);
}