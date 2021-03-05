struct VertexOut
{
	float3 PositionV : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL_WORLD;
	float3 NormalV   : NORMAL_VIEW;
	float2 TexCoord  : TEXCOORD;
};

Texture2D gAlbedoTexture : register(t0);
SamplerState gLinearSamplerState : register(s0);

float4 main(VertexOut pin) : SV_TARGET
{
	// TODO : enable normap mapping ?
	pin.NormalV = normalize(pin.NormalV);

#if ENABLE_TEXTURE
	float4 TextureColor = gAlbedoTexture.Sample(gLinearSamplerState, pin.TexCoord);
#if ENABLE_ALPHA_CLIPPING
	clip(TextureColor.a - 0.1f);
#endif // ENABLE_ALPHA_CLIPPING
#endif // ENABLE_TEXTURE

	return float4(pin.NormalW, pin.PositionH.z);
}