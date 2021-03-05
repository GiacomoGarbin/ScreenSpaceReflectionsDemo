struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
	float2 TexCoord  : TEXCOORD;
};

struct VertexOut
{
	float3 PositionV : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL_WORLD;
	float3 NormalV   : NORMAL_VIEW;
	float2 TexCoord  : TEXCOORD;
};

cbuffer NormalDepthCB : register(b0)
{
	float4x4 gWorldView;
	float4x4 gWorldViewProj;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldInverseTransposeView;
	float4x4 gTexCoordTransform;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionV = mul(gWorldView, float4(vin.PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.NormalW = mul((float3x3)gWorldInverseTranspose, vin.NormalL);
	vout.NormalV = mul((float3x3)gWorldInverseTransposeView, vin.NormalL);
	vout.TexCoord = mul(gTexCoordTransform, float4(vin.TexCoord, 0, 1)).xy;
	
	return vout;
}