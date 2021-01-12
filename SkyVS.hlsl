struct Material
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 reflect;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexTransform;
};

struct VertexIn
{
	float3 PositionL : POSITION;
};

struct VertexOut
{
	float3 PositionL : POSITION;
	float4 PositionH : SV_POSITION;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionL = vin.PositionL;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1)).xyww;

	return vout;
}