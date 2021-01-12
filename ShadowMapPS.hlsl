Texture2D gAlbedoTexture : register(t0);
SamplerState gLinearSamplerState : register(s0);

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord  : TEXCOORD;
};

void main(VertexOut pin)
{
#if ENABLE_TEXTURE
	float4 TextureColor = gAlbedoTexture.Sample(gLinearSamplerState, pin.TexCoord);
#if ENABLE_ALPHA_CLIPPING
	clip(TextureColor.a - 0.1f);
#endif // ENABLE_ALPHA_CLIPPING
#endif // ENABLE_TEXTURE
}