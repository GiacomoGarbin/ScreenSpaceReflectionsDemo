cbuffer ConstantBuffer : register(b0)
{
	float4   gFrustumFarCorner[4]; // just need 3 floats : half width, half height and far z (maybe near z too)
	float4x4 gProj;

	float4x4 gView;
	float4x4 gViewInverse;
	float4x4 gReflect;
};

struct VertexOut
{
	float4 PositionH  : SV_POSITION;
	float3 ToFarPlane : TEXCOORD0;
	float2 TexCoord   : TEXCOORD1;
};

Texture2D gNormalDepthMap : register(t0);
SamplerState gNormalDepthSamplerState : register(s2);

float3 GetFullViewPosition(float2 uv, float z)
{
	// bilinear interpolation
	float4 p0 = lerp(gFrustumFarCorner[0], gFrustumFarCorner[3], uv.x);
	float4 p1 = lerp(gFrustumFarCorner[1], gFrustumFarCorner[2], uv.x);
	float3 ToFarPlane = lerp(p0.xyw, p1.xyw, 1 - uv.y);

	// reconstruct full view space position (x,y,z)
	// find t such that p = t*pin.tofarplane
	// p.z = t*pin.tofarplane.z ==> t = p.z / pin.tofarplane.z
	return (z / gFrustumFarCorner[0].w) * ToFarPlane;
}

float3 GetFullViewPosition(float2 uv)
{
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, uv, 0);
	return GetFullViewPosition(uv, NormalDepth.w);
}

RWTexture2D<uint> gReflectionsMap : register(u1);

// http://remi-genin.fr/blog/screen-space-plane-indexed-reflection-in-ghost-recon-wildlands/

float4 main(VertexOut pin) : SV_Target
{
	//float DepthScale = 0.05f;

	// view space normal and depth (z-coord) of this pixel
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
	float3 normal = normalize(NormalDepth.xyz);
	float  depth  = NormalDepth.w;
	//return float4(NormalDepth.www * DepthScale, 0);

	float3 PosVS = GetFullViewPosition(pin.TexCoord, depth);
	//return float4(PosVS, 0);
	//return float4(PosVS.zzz * DepthScale, 0);

	float3 PosWS = mul(gViewInverse, float4(PosVS, 1)).xyz;
	//return float4(PosWS, 0);

	float3 ReflPosWS = mul(gReflect, float4(PosWS, 1)).xyz;
	//return float4(HitPointWS, 0);

	float3 ReflPosVS = mul(gView, float4(ReflPosWS, 1)).xyz;
	//return float4(HitPointVS, 0);
	//return float4(HitPointVS.zzz * DepthScale, 0);

	float2 TextureSize;
	gNormalDepthMap.GetDimensions(TextureSize.x, TextureSize.y);

	float4 ReflPixel = mul(gProj, float4(ReflPosVS, 1));
	ReflPixel.xy /= ReflPixel.w;
	ReflPixel.xy = ReflPixel.xy * float2(+0.5f, -0.5f) + 0.5f;
	ReflPixel.xy *= TextureSize;

	////HitPixel.y = 1 - HitPixel.y;

	//float visibility = (any(HitPixel.xy < float2(0, 0)) || any(HitPixel.xy > float2(1, 1))) ? 0 : 1;

	//return float4(HitPixel.xy, 0, visibility);


	//float3 PosWS = Unproject(ScreenUV, MainDepthBuffer);
	//float3 ReflPosWS = float3(PosWS.xy, 2 * WaterHeight – PosWS.z);
	//float2 ReflPosUV = Project(ReflPosWS);

	//uint2 SrcPosPixel = ScreenUV * FrameSize;
	//uint2 ReflPosPixel = ReflPosUV * FrameSize;

	//ProjectionHashUAV[ReflPosPixel] = SrcPosPixel.y << 16 | SrcPosPixel.x;

	//gReflectionsMap[pin.PositionH.xy] = 255;
	//gReflectionsMap[ReflPixel.xy] = uint(pin.PositionH.y) << 16 | uint(pin.PositionH.x);

	uint temp;
	InterlockedMax(gReflectionsMap[ReflPixel.xy], uint(pin.PositionH.y) << 16 | uint(pin.PositionH.x), temp);

	return 0;
}