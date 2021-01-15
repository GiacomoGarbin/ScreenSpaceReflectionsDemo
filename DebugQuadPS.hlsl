Texture2D gAlbedoTexture : register(t0);
#if ENABLE_SSR
Texture2D gReflectionsMapTexture : register(t1);
#endif // ENABLE_SSR
SamplerState gSamplerState;

struct VertexOut
{
    float4 PositionH : SV_POSITION;
    float2 TexCoord  : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    float3 albedo = gAlbedoTexture.Sample(gSamplerState, pin.TexCoord).rgb;

#if ENABLE_SSR
    float2 uv = gReflectionsMapTexture.Sample(gSamplerState, pin.TexCoord).xy;
    float3 reflection = gAlbedoTexture.Sample(gSamplerState, uv).rgb;
    albedo = lerp(albedo, reflection, 1);
#endif // ENABLE_SSR

    return float4(albedo, 1);
}
