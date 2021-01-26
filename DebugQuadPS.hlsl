cbuffer DebugQuadCB : register(b0)
{
    float4x4 gWorldViewProj;
    float2 gDebugQuadSize;
    float2 padding;
};

#if ENABLE_SSPR
Texture2D<uint> gAlbedoTexture : register(t0);
#else // ENABLE_SSPR
Texture2D gAlbedoTexture : register(t0);
#endif // ENABLE_SSPR

SamplerState gSamplerState;

struct VertexOut
{
    float4 PositionH : SV_POSITION;
    float2 TexCoord  : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    float3 albedo = 0;

#if ENABLE_SSPR
    //uint value = gAlbedoTexture[pin.PositionH.xy];
    uint value = gAlbedoTexture.Load(int3(pin.TexCoord * gDebugQuadSize, 0));
    //uint value = gAlbedoTexture.Sample(gSamplerState, pin.TexCoord);
    //albedo = value / 255.0f;
    //albedo = value;

    uint x = value & 0xFFFF;
    uint y = value >> 16;

    if (value != 0)
    {
        albedo = float3(float2(x, y) / float2(800, 600), 0);
    }

    //float2 size = float2(800, 600);
    //albedo = float3(pin.PositionH.xy / size, 0);

#else // ENABLE_SSPR
    albedo = gAlbedoTexture.Sample(gSamplerState, pin.TexCoord).rgb;
#endif // ENABLE_SSPR

    return float4(albedo, 1);
}
