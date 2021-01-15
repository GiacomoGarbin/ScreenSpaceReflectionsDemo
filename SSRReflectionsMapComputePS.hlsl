cbuffer ConstantBuffer : register(b0)
{
	float4   gFrustumFarCorner[4]; // just need 3 floats : half width, half height and far z
	float4x4 gProj;
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
	float3 ToFarPlane = lerp(p0.xyz, p1.xyz, 1 - uv.y);

	return (z / ToFarPlane.z) * ToFarPlane;
}

float3 GetFullViewPosition(float2 uv)
{
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, uv, 0);
	return GetFullViewPosition(uv, NormalDepth.w);
}

// https://github.com/FlaxEngine/FlaxEngine/blob/master/Source/Shaders/SSR.shader

float4 main(VertexOut pin) : SV_Target
{
	// view space normal and depth (z-coord) of this pixel
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);

	float3 n = NormalDepth.xyz;
	float pz = NormalDepth.w;

	// reconstruct full view space position (x,y,z)
	// find t such that p = t*pin.ToFarPlane
	// p.z = t*pin.ToFarPlane.z ==> t = p.z / pin.ToFarPlane.z
	//float3 p = (pz / pin.ToFarPlane.z) * pin.ToFarPlane;
	float3 p = GetFullViewPosition(pin.TexCoord, pz);

	{
		float MaxDistance = 15;
		float resolution = 0.3f;
		int   steps = 10;
		float thickness = 0.5f;

		float2 TexSize;
		gNormalDepthMap.GetDimensions(TexSize.x, TexSize.y);
		float2 TexCoord = pin.PositionH.xy / TexSize;

		//return float4(TexCoord, 0, 1);

		float3 PositionFrom = p;
		float3 PositionFromUnit = normalize(PositionFrom);
		float3 normal = normalize(n);
		float3 pivot = normalize(reflect(PositionFromUnit, normal));

		float3 PositionTo = PositionFrom;

		//return float4(PositionFrom, 1);
		//return float4(PositionFromUnit, 1);
		//return float4(normal, 1);
		//return float4(pivot, 1);
		//return float4(PositionTo, 1);

		float4 uv = 0;

		float3 ViewStart = PositionFrom + pivot * 0;
		float3 ViewEnd   = PositionFrom + pivot * MaxDistance;

		float4 FragStart = float4(ViewStart, 1);
		FragStart = mul(gProj, FragStart);
		FragStart.xy /= FragStart.w;
		FragStart.xy = FragStart.xy * float2(+0.5f, -0.5f) + 0.5f;
		//return float4(FragStart.xy, 0, 1);
		FragStart.xy *= TexSize;

		float4 FragEnd = float4(ViewEnd, 1);
		FragEnd = mul(gProj, FragEnd);
		FragEnd.xy /= FragEnd.w;
		FragEnd.xy = FragEnd.xy * float2(+0.5f, -0.5f) + 0.5f;
		//return float4(FragEnd.xy, 0, 1);
		FragEnd.xy *= TexSize;

		float2 frag = FragStart.xy;
		uv.xy = frag / TexSize;

		float DeltaX = FragEnd.x - FragStart.x;
		float DeltaY = FragEnd.y - FragStart.y;
		float UseX = abs(DeltaX) >= abs(DeltaY) ? 1 : 0;
		float delta = lerp(abs(DeltaY), abs(DeltaX), UseX) * clamp(resolution, 0, 1);
		float2 increment = float2(DeltaX, DeltaY) / max(delta, 0.001f);

		float search0 = 0;
		float search1 = 0;

		int hit0 = 0;
		int hit1 = 0;

		float ViewDistance = ViewStart.z;
		float depth = thickness;

		int j = 0;

		for (int i = 0; i < int(delta); ++i)
		{
			frag += increment;
			uv.xy = frag / TexSize;
			PositionTo = GetFullViewPosition(uv.xy);

			search1 = lerp((frag.y - FragStart.y) / DeltaY, (frag.x - FragStart.x) / DeltaX, UseX);
			search1 = clamp(search1, 0, 1);

			ViewDistance = (ViewStart.z * ViewEnd.z) / lerp(ViewEnd.z, ViewStart.z, search1);
			depth = ViewDistance - PositionTo.z;

			if (depth > 0 && depth < thickness)
			{
				hit0 = 1;
				break;
			}
			else
			{
				search0 = search1;
			}

			
			if (++j >= 100) break;
		}

		search1 = search0 + ((search1 - search0) / 2);

		steps *= hit0;

		for (int i = 0; i < steps; ++i)
		{
			frag = lerp(FragStart.xy, FragEnd.xy, search1);
			uv.xy = frag / TexSize;
			PositionTo = GetFullViewPosition(uv.xy);

			ViewDistance = (ViewStart.z * ViewEnd.z) / lerp(ViewEnd.z, ViewStart.z, search1);
			depth = ViewDistance - PositionTo.z;

			if (depth > 0 && depth < thickness)
			{
				hit1 = 1;
				search1 = search0 + ((search1 - search0) / 2);
			}
			else
			{
				float temp = search1;
				search1 = search1 + ((search1 - search0) / 2);
				search0 = temp;
			}
		}

		return float4(uv.xy, 1, 1);
	}

	return float4(p, 1);
}

struct RayPayload
{
	bool hit;
	float2 TexCoord;
};

//float4 main(VertexOut pin) : SV_Target
//{
//	// view space normal and depth (z-coord) of this pixel
//	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
//
//	float3 n = NormalDepth.xyz;
//	float pz = NormalDepth.w;
//
//	float3 p = GetFullViewPosition(pin.TexCoord, pz);
//
//
//	float3 dir = normalize(p);
//	float3 ReflectDir = normalize(reflect(dir, normalize(n)));
//
//
//
//	float3 CurrPosition = 0;
//	float3 CurrTexCoord = 0;
//
//	float dl = 0.05f;
//	float CurrLength = dl;
//
//	float DepthBias = 0.025f;
//
//	RayPayload payload;
//	payload.hit = false;
//	payload.TexCoord = float2(0, 0);
//
//	for (int i = 0; i < 512; ++i)
//	{
//		//if (!payload.hit)
//		{
//			CurrPosition = p + ReflectDir * CurrLength;
//
//			float4 temp = float4(CurrPosition, 1);
//			temp = mul(gProj, temp);
//			temp.xy /= temp.w;
//			CurrTexCoord = float3(temp.xy * float2(+0.5f, -0.5f) + 0.5f, temp.z);
//
//			//CurrTexCoord = temp.z / temp.w;
//			//return float4(CurrTexCoord, 1);
//
//
//			// view space depth -> [0,INF)
//			float CurrZ = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, CurrTexCoord.xy, 0).w;
//
//			if (abs(CurrPosition.z - CurrZ) < DepthBias)
//			{
//				payload.hit = true;
//				payload.TexCoord = CurrTexCoord.xy;
//				break;
//			}
//
//			//float3 NextPosition = GetFullViewPosition(CurrTexCoord.xy, CurrZ);
//			//CurrLength = length(p - NextPosition);
//			CurrLength += dl;
//		}
//	}
//
//	return float4(payload.TexCoord, payload.hit ? 1 : 0, 1);
//}