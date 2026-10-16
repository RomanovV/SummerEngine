Texture2D gAlbedo   : register(t0);
Texture2D gNormal   : register(t1);
Texture2D gMaterial : register(t2);
Texture2D gDepth    : register(t3);
SamplerState gSampler : register(s0);

cbuffer DebugViewConstants : register(b0)
{
    uint mode;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float4 albedo = gAlbedo.Sample(gSampler, input.uv);
    float3 normal = gNormal.Sample(gSampler, input.uv).xyz;
    float3 material = gMaterial.Sample(gSampler, input.uv).rgb;
    float depth = gDepth.Sample(gSampler, input.uv).r;

    float3 color = 0.0f;
    if (mode == 0u) {
        color = albedo.rgb;
    } else if (mode == 1u) {
        color = saturate(normal * 0.5f + 0.5f);
    } else if (mode == 2u) {
        color = albedo.aaa;
    } else if (mode == 3u) {
        color = material.rrr;
    } else if (mode == 4u) {
        color = material.ggg;
    } else if (mode == 5u) {
        color = material.bbb;
    } else {
        color = (depth >= 0.99999f) ? 0.0f : pow(saturate(depth), 24.0f);
    }

    return float4(color, 1.0f);
}
