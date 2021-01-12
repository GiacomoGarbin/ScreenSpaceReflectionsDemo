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

cbuffer cbPerSkinned : register(b2)
{
	float4x4 gBoneTransforms[96];
};

struct VertexIn
{
	float3 PositionL  : POSITION;
	float3 NormalL    : NORMAL;
	float3 TangentL   : TANGENT;
	float2 TexCoord   : TEXCOORD;
	float3 weights    : WEIGHTS;
	uint4 BoneIndices : BONEINDICES;
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

	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	weights[0] = vin.weights.x;
	weights[1] = vin.weights.y;
	weights[2] = vin.weights.z;
	weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

	float3 PositionL = float3(0.0f, 0.0f, 0.0f);
	float3 NormalL   = float3(0.0f, 0.0f, 0.0f);
	float3 TangentL  = float3(0.0f, 0.0f, 0.0f);
	
	for (int i = 0; i < 4; ++i)
	{
		PositionL += weights[i] * mul(gBoneTransforms[vin.BoneIndices[i]], float4(vin.PositionL, 1.0f)).xyz;
		NormalL   += weights[i] * mul((float3x3)gBoneTransforms[vin.BoneIndices[i]], vin.NormalL);
		TangentL  += weights[i] * mul((float3x3)gBoneTransforms[vin.BoneIndices[i]], vin.TangentL);
	}

	vout.PositionW = mul(gWorld, float4(PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(PositionL, 1));
	vout.NormalW = mul((float3x3)gWorldInverseTranspose, NormalL);
	vout.TangentW = mul((float3x3)gWorld, TangentL);
	vout.TexCoord = mul(gTexCoordTransform, float4(vin.TexCoord, 0, 1)).xy;
	vout.ShadowH = mul(gShadowTransform, float4(PositionL, 1));
	vout.AmbientH = mul(gWorldViewProjTexture, float4(PositionL, 1));

	float d = distance(vout.PositionW, gEyePositionW);
	float f = saturate((gMinTessDistance - d) / (gMinTessDistance - gMaxTessDistance));
	// from [0,1] to [gMinTessFactor,gMaxTessFactor]
	vout.TessFactor = gMinTessFactor + f * (gMaxTessFactor - gMinTessFactor);

	return vout;
}