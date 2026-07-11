struct PSInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float4 color    : COLOR0;
};

float4 main(PSInput input) : SV_Target
{
    // Simple directional light — tweak the light direction or swap this for your experiments.
    float3 N = normalize(input.normal);
    float3 L = normalize(float3(0.4f, 0.8f, -0.3f));
    float ndotl = saturate(dot(N, L));
    float3 ambient = 0.15f * input.color.rgb;
    float3 diffuse = 0.85f * ndotl * input.color.rgb;
    return float4(ambient + diffuse, 1.0f);
}
