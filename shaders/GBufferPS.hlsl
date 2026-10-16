cbuffer MaterialConstants : register(b2)
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float _matPad;
};

Texture2D gBaseColor  : register(t0);
Texture2D gNormal     : register(t1);
Texture2D gMetalRough : register(t2); // ORM: R=occlusion, G=roughness, B=metallic
Texture2D gOcclusion  : register(t3);
Texture2D gEmissive   : register(t4);
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 clipPos  : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
};

float3 GetNormal(PSInput input)
{
    float3 tangentNormal = gNormal.Sample(gSampler, input.uv).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= normalScale;

    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz - N * dot(N, input.tangent.xyz));
    float3 B = cross(N, T) * input.tangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(tangentNormal, TBN));
}

struct GBufferOutput {
    float4 albedo   : SV_Target0; // RGB linear albedo, A occlusion
    float4 normal   : SV_Target1; // RGB world normal, unencoded
    float4 material : SV_Target2; // R metallic, G roughness, B emissive luminance
};

GBufferOutput main(PSInput input)
{
    float4 baseColorSample = gBaseColor.Sample(gSampler, input.uv) * baseColorFactor;
    float3 orm = gMetalRough.Sample(gSampler, input.uv).rgb;
    float roughness = saturate(orm.g * roughnessFactor);
    float metallic = saturate(orm.b * metallicFactor);
    
    float occlusion = gOcclusion.Sample(gSampler, input.uv).r;
    occlusion = lerp(1.0f, occlusion, occlusionStrength);
    
    float3 emissive = gEmissive.Sample(gSampler, input.uv).rgb * emissiveFactor;
    float emissiveLum = dot(emissive, float3(0.2126f, 0.7152f, 0.0722f));
    
    GBufferOutput output;
    output.albedo = float4(baseColorSample.rgb, occlusion);
    output.normal = float4(GetNormal(input), 0.0f);
    output.material = float4(metallic, roughness, emissiveLum, 1.0f);
    return output;
}