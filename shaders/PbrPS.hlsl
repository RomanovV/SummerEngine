// Forward PBR: Cook-Torrance metallic-roughness + tangent-space normal mapping.
// Outputs linear HDR radiance; tonemapping/gamma happens in the tonemap pass.

static const float PI = 3.14159265359f;
static const uint kMaxPointLights = 4;

struct DirectionalLight
{
    float3 direction;
    float intensity;
    float3 color;
    float _pad;
};

struct PointLight
{
    float3 position;
    float radius;
    float3 color;
    float intensity;
};

cbuffer FrameConstants : register(b1)
{
    float4x4 viewProj;
    float3 cameraPos;
    float _pad0;
    DirectionalLight dirLight;
    float3 ambient;
    int pointLightCount;
    PointLight pointLights[kMaxPointLights];
};

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

#include "PbrCommon.hlsli"

float4 main(PSInput input) : SV_Target
{
    float4 baseColorSample = gBaseColor.Sample(gSampler, input.uv) * baseColorFactor;
    float3 albedo = baseColorSample.rgb;

    float3 orm = gMetalRough.Sample(gSampler, input.uv).rgb;
    float roughness = saturate(orm.g * roughnessFactor);
    float metallic = saturate(orm.b * metallicFactor);

    // Occlusion may come from a dedicated map or the ORM red channel.
    float occlusion = gOcclusion.Sample(gSampler, input.uv).r;
    occlusion = lerp(1.0f, occlusion, occlusionStrength);

    float3 N = GetNormal(input);
    float3 V = normalize(cameraPos - input.worldPos);

    float3 color = 0.0f;

    // Directional light.
    {
        float3 L = normalize(-dirLight.direction);
        float3 radiance = dirLight.color * dirLight.intensity;
        color += BRDF(N, V, L, radiance, albedo, metallic, roughness);
    }

    // Point lights (inverse-square falloff with a smooth radius cutoff).
    for (int i = 0; i < pointLightCount; ++i)
    {
        float3 toLight = pointLights[i].position - input.worldPos;
        float distance = length(toLight);
        float3 L = toLight / max(distance, 1e-4f);
        float attenuation = 1.0f / max(distance * distance, 1e-4f);
        float window = saturate(1.0f - pow(distance / max(pointLights[i].radius, 1e-4f), 4.0f));
        attenuation *= window * window;
        float3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        color += BRDF(N, V, L, radiance, albedo, metallic, roughness);
    }

    // Ambient (constant) + emissive.
    float3 ambientTerm = ambient * albedo * occlusion;
    float3 emissive = gEmissive.Sample(gSampler, input.uv).rgb * emissiveFactor;

    float3 finalColor = color * occlusion + ambientTerm + emissive;
    return float4(finalColor, baseColorSample.a);
}
