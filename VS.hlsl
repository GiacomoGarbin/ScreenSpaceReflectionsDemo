struct Material
{
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float4 reflect;
};

struct LightDirectional
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 direction;
	float  pad;
};

struct LightPoint
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float  range;
	float3 attenuation;
	float  pad;
};

struct LightSpot
{
	float4 ambient;
	float4 diffuse;
	float4 specular;

	float3 position;
	float  range;
	float3 direction;
	float  cone;
	float3 attenuation;
	float  pad;
};

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInverseTranspose;
	float4x4 gWorldViewProj;
	Material gMaterial;
	float4x4 gTexCoordTransform;
	float4x4 gShadowTransform;
	float4x4 gWorldViewProjTexture;
};

cbuffer cbPerFrame : register(b1)
{
	LightDirectional gLights[3];

	float3 gEyePositionW;
	float  pad1;

	float  gFogStart;
	float  gFogRange;
	float2 pad2;
	float4 gFogColor;

	float gHeightScale;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
	float3 pad3;

	float4x4 gViewProj;
};

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
	float3 TangentL  : TANGENT;
	float2 TexCoord  : TEXCOORD;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
	float2 TexCoord  : TEXCOORD0;
	float4 ShadowH   : TEXCOORD1;
	float4 AmbientH  : TEXCOORD2;
	float TessFactor : TESSFACTOR;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PositionW = mul(gWorld, float4(vin.PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(vin.PositionL, 1));
	vout.NormalW   = mul((float3x3)gWorldInverseTranspose, vin.NormalL);
	vout.TangentW  = mul((float3x3)gWorld, vin.TangentL);
	vout.TexCoord  = mul(gTexCoordTransform, float4(vin.TexCoord, 0, 1)).xy;
	vout.ShadowH   = mul(gShadowTransform, float4(vin.PositionL, 1));
	vout.AmbientH  = mul(gWorldViewProjTexture, float4(vin.PositionL, 1));

	float d = distance(vout.PositionW, gEyePositionW);
	float f = saturate((gMinTessDistance - d) / (gMinTessDistance - gMaxTessDistance));
	// from [0,1] to [gMinTessFactor,gMaxTessFactor]
	vout.TessFactor = gMinTessFactor + f * (gMaxTessFactor - gMinTessFactor);

	return vout;
}