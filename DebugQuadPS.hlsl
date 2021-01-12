Texture2D gAlbedoTexture : register(t0);
SamplerState gSamplerState;

struct VertexOut
{
    float4 PositionH : SV_POSITION;
    float2 TexCoord  : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    float3 depth = gAlbedoTexture.Sample(gSamplerState, pin.TexCoord).r;
    return float4(depth, 1);
}
