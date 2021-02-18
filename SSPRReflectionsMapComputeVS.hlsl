cbuffer ConstantBuffer : register(b0)
{
	float4   gFrustumFarCorner[4];
	float4x4 gProj;

	float4x4 gView;
	float4x4 gViewInverse;
	float4x4 gReflect;

	float4x4 gViewProj;
	float4x4 gViewProjInverse;

	float3 gCameraPosition;

	float padding;
};

struct VertexIn
{
	float3 PositionL       : POSITION;
	float3 ToFarPlaneIndex : NORMAL;
	float2 TexCoord        : TEXCOORD;
};

struct VertexOut
{
	float4 PositionH  : SV_POSITION;
	float3 ToFarPlane : TEXCOORD0;
	float2 TexCoord   : TEXCOORD1;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// already in NDC space
	vout.PositionH = float4(vin.PositionL, 1.0f);
	// we store the index to the frustum corner in the normal x-coord slot
	vout.ToFarPlane = gFrustumFarCorner[vin.ToFarPlaneIndex.x].xyz;
	vout.TexCoord = vin.TexCoord;

	return vout;
}