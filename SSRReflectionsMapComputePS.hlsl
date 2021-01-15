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

float4 main(VertexOut pin) : sv_target
{
	// view space normal and depth (z-coord) of this pixel
	float4 normaldepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);

	float3 n = normaldepth.xyz;
	float pz = normaldepth.w;

	// reconstruct full view space position (x,y,z)
	// find t such that p = t*pin.tofarplane
	// p.z = t*pin.tofarplane.z ==> t = p.z / pin.tofarplane.z
	//float3 p = (pz / pin.tofarplane.z) * pin.tofarplane;
	float3 p = GetFullViewPosition(pin.TexCoord, pz);

	{
		float maxdistance = 100;
		float resolution = 1;
		int   steps = 100;
		float thickness = 0.5f;

		float2 texsize;
		gNormalDepthMap.GetDimensions(texsize.x, texsize.y);
		float2 texcoord = pin.PositionH.xy / texsize;

		//return float4(texcoord, 0, 1);

		float3 positionfrom = p;
		float3 positionfromunit = normalize(positionfrom);
		float3 normal = normalize(n);
		float3 pivot = normalize(reflect(positionfromunit, normal));

		float3 positionto = positionfrom;

		//return float4(positionfrom, 1);
		//return float4(positionfromunit, 1);
		//return float4(normal, 1);
		//return float4(pivot, 1);
		//return float4(positionto, 1);

		float4 uv = 0;

		float3 viewstart = positionfrom + pivot * 0;
		float3 viewend   = positionfrom + pivot * maxdistance;

		float4 fragstart = float4(viewstart, 1);
		fragstart = mul(gProj, fragstart);
		fragstart.xy /= fragstart.w;
		fragstart.xy = fragstart.xy * float2(+0.5f, -0.5f) + 0.5f;
		//return float4(fragstart.xy, 0, 1);
		fragstart.xy *= texsize;

		float4 fragend = float4(viewend, 1);
		fragend = mul(gProj, fragend);
		fragend.xy /= fragend.w;
		fragend.xy = fragend.xy * float2(+0.5f, -0.5f) + 0.5f;
		//return float4(fragend.xy, 0, 1);
		fragend.xy *= texsize;

		float2 frag = fragstart.xy;
		uv.xy = frag / texsize;

		float deltax = fragend.x - fragstart.x;
		float deltay = fragend.y - fragstart.y;
		float usex = abs(deltax) >= abs(deltay) ? 1 : 0;
		float delta = lerp(abs(deltay), abs(deltax), usex) * clamp(resolution, 0, 1);
		float2 increment = float2(deltax, deltay) / max(delta, 0.001f);

		float search0 = 0;
		float search1 = 0;

		int hit0 = 0;
		int hit1 = 0;

		float viewdistance = viewstart.z;
		float depth = thickness;

		int j = 0;

		for (int i = 0; i < int(delta); ++i)
		{
			frag += increment;
			uv.xy = frag / texsize;

			if (any(uv.xy < float2(0, 0)) || any(uv.xy > float2(1, 1))) break;

			positionto = GetFullViewPosition(uv.xy);

			search1 = lerp((frag.y - fragstart.y) / deltay, (frag.x - fragstart.x) / deltax, usex);
			search1 = clamp(search1, 0, 1);

			viewdistance = (viewstart.z * viewend.z) / lerp(viewend.z, viewstart.z, search1);
			//viewdistance = (viewstart.z * viewend.z) / lerp(viewstart.z, viewend.z, search1);
			depth = viewdistance - positionto.z;

			if (depth > 0 && depth < thickness)
			{
				hit0 = 1;
				break;
			}
			else
			{
				search0 = search1;
			}
			
			//if (++j >= 400) break;
		}

		//return float4(uv.xy, hit0, 1);

		search1 = search0 + ((search1 - search0) / 2);

		steps *= hit0;

		//thickness = 1;

		for (int i = 0; i < steps; ++i)
		{
			frag = lerp(fragstart.xy, fragend.xy, search1);
			uv.xy = frag / texsize;

			if (any(uv.xy < float2(0, 0)) || any(uv.xy > float2(1, 1))) break;

			positionto = GetFullViewPosition(uv.xy);

			viewdistance = (viewstart.z * viewend.z) / lerp(viewend.z, viewstart.z, search1);
			depth = viewdistance - positionto.z;

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

		//return float4(uv.xy, hit1, 1);

		float visibility = hit1 *
			(1 - max(dot(-positionfromunit, pivot), 0)) *
			(1 - clamp(depth / thickness, 0, 1)) *
			(1 - clamp(length(positionto - positionfrom) / maxdistance, 0, 1)) *
			(uv.x < 0 || uv.x > 1 ? 0 : 1) *
			(uv.y < 0 || uv.y > 1 ? 0 : 1);

		visibility = saturate(visibility);


		return float4(uv.xy, 0, visibility);

	}

	return float4(p, 1);
}

struct RayPayload
{
	bool hit;
	float2 TexCoord;
};

// TODO
// ray marching
// binary search
// blur

//
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
//	float angle = clamp(dot(ReflectDir, normalize(n)), 0, 1);
//
//
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
//	for (int i = 0; i < 1000; ++i)
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
//			if (any(CurrTexCoord.xy <= float2(0, 0)) || any(CurrTexCoord.xy >= float2(1, 1))) break;
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
//	//return float4(angle, angle, payload.hit ? 1 : 0, 1);
//	//return float4(payload.TexCoord, payload.hit ? 1 : 0, 1);
//
//	bool visibility = payload.hit && angle < 1;
//
//	return float4(payload.TexCoord, visibility ? 1 : 0, 1);
//}