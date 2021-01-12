
cbuffer ShadowMapCB : register(b0)
{
	float4x4 gWorldViewProj;
	float4x4 gTexTransform;
};

struct VertexIn
{
	float3 PositionL : POSITION;
	float2 TexCoord  : TEXCOORD;
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord  : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.TexCoord = mul(gTexTransform, float4(vin.TexCoord, 0, 1)).xy;

	return vout;
}