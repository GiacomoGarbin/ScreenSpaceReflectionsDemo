struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL   : NORMAL;
	float2 TexCoord  : TEXCOORD;
	float3 weights    : WEIGHTS;
	uint4 BoneIndices : BONEINDICES;
};

struct VertexOut
{
	float3 PositionV : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalV   : NORMAL;
	float2 TexCoord  : TEXCOORD;
};

cbuffer NormalDepthCB : register(b0)
{
	float4x4 gWorldView;
	float4x4 gWorldViewProj;
	float4x4 gWorldInverseTransposeView;
	float4x4 gTexCoordTransform;
};

cbuffer cbPerSkinned : register(b2)
{
	float4x4 gBoneTransforms[96];
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
	float3 NormalL = float3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < 4; ++i)
	{
		PositionL += weights[i] * mul(gBoneTransforms[vin.BoneIndices[i]], float4(vin.PositionL, 1.0f)).xyz;
		NormalL += weights[i] * mul((float3x3)gBoneTransforms[vin.BoneIndices[i]], vin.NormalL);
	}

	vout.PositionV = mul(gWorldView, float4(PositionL, 1)).xyz;
	vout.PositionH = mul(gWorldViewProj, float4(PositionL, 1));
	vout.NormalV = mul((float3x3)gWorldInverseTransposeView, NormalL);
	vout.TexCoord = mul(gTexCoordTransform, float4(vin.TexCoord, 0, 1)).xy;

	return vout;
}