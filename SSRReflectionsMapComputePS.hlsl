cbuffer ConstantBuffer : register(b0)
{
	float4   gFrustumFarCorner[4]; // just need 3 floats : half width, half height and far z (maybe near z too)
	float4x4 gProj;

	float4x4 gView;
	float4x4 gViewInverse;
	float4x4 gReflect;

	float4x4 gViewProj;
	float4x4 gViewProjInverse;

	float4x4 gProjInverse;

	float3 gCameraPosition;

	float padding;
};

struct VertexOut
{
	float4 PositionH  : SV_POSITION;
	float3 ToFarPlane : TEXCOORD0;
	float2 TexCoord   : TEXCOORD1;
};

Texture2D gNormalDepthMap : register(t0);
SamplerState gNormalDepthSamplerState : register(s2);

Texture2D<float> gHierarchicalDepthBuffer : register(t1);
SamplerState gHierarchicalDepthBufferSamplerState : register(s3);

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

// 1st ATTEMPT
// https://lettier.github.io/3d-game-shaders-for-beginners/screen-space-reflection.html
// https://github.com/lettier/3d-game-shaders-for-beginners/blob/master/demonstration/shaders/fragment/screen-space-reflection.frag

//float4 main(VertexOut pin) : sv_target
//{
//	// view space normal and depth (z-coord) of this pixel
//	float4 normaldepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
//
//	float3 n = normaldepth.xyz;
//	float pz = normaldepth.w;
//
//	//float3 p = (pz / pin.tofarplane.z) * pin.tofarplane;
//	float3 p = GetFullViewPosition(pin.TexCoord, pz);
//
//	float maxdistance = 100;
//	float resolution = 1;
//	float steps = 10;
//	float thickness = 0.05f;
//
//	float2 texsize;
//	gNormalDepthMap.GetDimensions(texsize.x, texsize.y);
//	float2 texcoord = pin.PositionH.xy / texsize;
//
//	float3 positionfrom = p;
//	float3 positionfromunit = normalize(positionfrom);
//	float3 normal = normalize(n);
//	float3 pivot = normalize(reflect(positionfromunit, normal));
//
//	float3 positionto = positionfrom;
//	float4 uv = 0;
//
//	{
//		// clip to the near plane
//		float NearPlaneZ = gFrustumFarCorner[0].z;
//
//		//float NearPlaneZ = (2 * gProj._m32) / (2 * gProj._m22 - 2);
//
//		//float FarPlaneZ = gProj._m32 / (gProj._m22 - 1);
//
//		//float NearPlaneZ = - gProj._m32 / gProj._m22;
//
//		maxdistance = ((positionfrom.z + pivot.z * maxdistance) < NearPlaneZ) ? (NearPlaneZ - positionfrom.z) / pivot.z : maxdistance;
//	}
//
//	float3 viewstart = positionfrom + pivot * 0;
//	float3 viewend   = positionfrom + pivot * maxdistance;
//
//	float4 fragstart = float4(viewstart, 1);
//	fragstart = mul(gProj, fragstart);
//	fragstart.xy /= fragstart.w;
//	fragstart.xy = fragstart.xy * float2(+0.5f, -0.5f) + 0.5f;
//	//return float4(fragstart.xy, 0, 1);
//	fragstart.xy *= texsize;
//
//	float4 fragend = float4(viewend, 1);
//	fragend = mul(gProj, fragend);
//	fragend.xy /= fragend.w;
//	fragend.xy = fragend.xy * float2(+0.5f, -0.5f) + 0.5f;
//	//return float4(fragend.xy, 0, 1);
//	fragend.xy *= texsize;
//
//	float2 frag = fragstart.xy;
//	uv.xy = frag / texsize;
//
//	float deltax = fragend.x - fragstart.x;
//	float deltay = fragend.y - fragstart.y;
//	float usex = abs(deltax) >= abs(deltay) ? 1 : 0;
//	float delta = lerp(abs(deltay), abs(deltax), usex) * clamp(resolution, 0, 1);
//	float2 increment = float2(deltax, deltay) / max(delta, 0.001f);
//
//	float search0 = 0;
//	float search1 = 0;
//
//	int hit0 = 0;
//	int hit1 = 0;
//
//	float viewdistance = viewstart.z;
//	float depth = thickness;
//
//	//float length = 0;
//
//	for (int i = 0; i < int(delta); ++i)
//	{
//		frag += increment;
//		uv.xy = frag / texsize;
//
//		// do not sample outside the screen space
//		if (any(uv.xy < float2(0, 0)) || any(uv.xy > float2(1, 1))) break;
//
//		positionto = GetFullViewPosition(uv.xy);
//
//		search1 = lerp((frag.y - fragstart.y) / deltay, (frag.x - fragstart.x) / deltax, usex);
//		search1 = clamp(search1, 0, 1);
//
//		viewdistance = (viewstart.z * viewend.z) / lerp(viewend.z, viewstart.z, search1);
//		depth = viewdistance - positionto.z;
//
//		if (depth > 0 && depth < thickness)
//		{
//			hit0 = 1;
//			break;
//		}
//		else
//		{
//			search0 = search1;
//		}
//	}
//
//	search1 = search0 + ((search1 - search0) / 2);
//
//	steps *= hit0;
//	hit1 = hit0;
//
//	for (int i = 0; i < steps; ++i)
//	{
//		//thickness *= 0.5f;
//
//		frag = lerp(fragstart.xy, fragend.xy, search1);
//		uv.xy = frag / texsize;
//
//		// do not sample outside the screen space
//		//if (any(uv.xy < float2(0, 0)) || any(uv.xy > float2(1, 1))) break;
//
//		positionto = GetFullViewPosition(uv.xy);
//
//		viewdistance = (viewstart.z * viewend.z) / lerp(viewend.z, viewstart.z, search1);
//		depth = viewdistance - positionto.z;
//
//		if (depth > 0 && depth < thickness)
//		{
//			//hit1 = 1;
//			search1 = search0 + ((search1 - search0) / 2);
//		}
//		else
//		{
//			float temp = search1;
//			search1 = search1 + ((search1 - search0) / 2);
//			search0 = temp;
//		}
//	}
//
//	{
//		float length = 10;
//		float3 p = float3(0, 0, 0) + float3(0, 0, 1) * length;
//
//		//hit1 = p.z > positionto.z ? 0 : 1;
//
//		//hit1 = positionto.z > viewdistance ? 0 : 1;
//	}
//
//	float visibility = hit1 *
//		(1 - max(dot(-positionfromunit, pivot), 0)) *
//		(1 - clamp(depth / thickness, 0, 1)) *
//		(1 - clamp(length(positionto - positionfrom) / maxdistance, 0, 1)) *
//		(uv.x < 0 || uv.x > 1 ? 0 : 1) *
//		(uv.y < 0 || uv.y > 1 ? 0 : 1);
//
//
//	//visibility *= 1 - smoothstep(-0.17, 0.0, dot(normal, -pivot));
//
//	//visibility = 1;
//
//	return float4(uv.xy, 0, saturate(visibility));
//}

// 2nd ATTEMPT
// https://virtexedgedesign.wordpress.com/2018/06/24/shader-series-basic-screen-space-reflections/

//struct RayPayload
//{
//	bool hit;
//	float2 TexCoord;
//};
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
//	float3 dir = normalize(p);
//	float3 ReflectDir = normalize(reflect(dir, normalize(n)));
//
//	float angle = clamp(dot(ReflectDir, normalize(n)), 0, 1);
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
//	return float4(payload.TexCoord, 0, visibility ? 1 : 0);
//}

// 3rd ATTEMPT
// http://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html
// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/ssr_raytraceCS.hlsl

//float distanceSquared(float2 a, float2 b) { a -= b; return dot(a, a); }
//
//static const float rayTraceMaxStep = 512.0f; // Maximum number of iterations. Higher gives better images but may be slow.
//static const float rayTraceThicknessOffset = 0.01f; // Increse or decrease thickness for each pixels in the depth buffer. [- / +]
//static const float rayTraceThicknessBias = 0.1f; // Bias to control the growth of the thickness.
//static const bool raytraceThicknessInfinite = false; // Use infinite thickness for maximum performance, but may not be suitable for most scenes.
//static const float rayTraceStrideCutoff = 100.0f; // More distant pixels are smaller in screen space. This value tells at what point to
//
//bool IntersectsDepthBuffer(float sceneZMax, float rayZMin, float rayZMax)
//{
//	// Increase thickness along distance. 
//	float thickness = max(sceneZMax * rayTraceThicknessBias + rayTraceThicknessOffset, 1.0);
//
//	// Effectively remove line/tiny artifacts, mostly caused by Zbuffers precision.
//	float depthScale = min(1.0f, sceneZMax / rayTraceStrideCutoff);
//	sceneZMax += lerp(0.05f, 0.0f, depthScale);
//
//	if (raytraceThicknessInfinite)
//		return (rayZMin >= sceneZMax);
//	else
//		return (rayZMin >= sceneZMax) && (rayZMax - thickness <= sceneZMax);
//}
//
//// Returns true if the ray hit something
//bool TraceScreenSpaceRay(
//	// Camera-space ray origin, which must be within the view volume
//	float3 csOrig,
//
//	// Unit length camera-space ray direction
//	float3 csDir,
//
//	// A projection matrix that maps to pixel coordinates (not [-1, +1]
//	// normalized device coordinates)
//	float4x4 proj,
//
//	// The camera-space Z buffer (all negative values)
//	Texture2D csZBuffer,
//
//	// Dimensions of csZBuffer
//	float2 csZBufferSize,
//
//	// Camera space thickness to ascribe to each pixel in the depth buffer
//	float zThickness,
//
//	// (Negative number)
//	float nearPlaneZ,
//
//	// Step in horizontal or vertical pixels between samples. This is a float
//	// because integer math is slow on GPUs, but should be set to an integer >= 1
//	float stride,
//
//	// Number between 0 and 1 for how far to bump the ray in stride units
//	// to conceal banding artifacts
//	float jitter,
//
//	// Maximum number of iterations. Higher gives better images but may be slow
//	const float maxSteps,
//
//	// Maximum camera-space distance to trace before returning a miss
//	float maxDistance,
//
//	// Pixel coordinates of the first intersection with the scene
//	out float2 hitPixel,
//
//	// Camera space location of the ray hit
//	out float3 hitPoint)
//{
//	hitPixel = 0;
//	hitPoint = 0;
//
//	// Clip to the near plane    
//	float rayLength = ((csOrig.z + csDir.z * maxDistance) < nearPlaneZ) ? (nearPlaneZ - csOrig.z) / csDir.z : maxDistance;
//	float3 csEndPoint = csOrig + csDir * rayLength;
//
//	// Project into homogeneous clip space
//	float4 H0 = mul(proj, float4(csOrig, 1.0f));
//	float4 H1 = mul(proj, float4(csEndPoint, 1.0f));
//
//	float k0 = 1.0f / H0.w;
//	float k1 = 1.0f / H1.w;
//
//	// The interpolated homogeneous version of the camera-space points  
//	float3 Q0 = csOrig * k0;
//	float3 Q1 = csEndPoint * k1;
//
//	// Screen-space endpoints
//	float2 P0 = H0.xy * k0;
//	float2 P1 = H1.xy * k1;
//
//	// Project to pixel
//	P0 = P0 * float2(+0.5f, -0.5f) + float2(0.5f, 0.5f);
//	P1 = P1 * float2(+0.5f, -0.5f) + float2(0.5f, 0.5f);
//
//	//return float4(P0, 0, 1);
//
//	P0.xy *= csZBufferSize.xy;
//	P1.xy *= csZBufferSize.xy;
//
//	// If the line is degenerate, make it cover at least one pixel
//	// to avoid handling zero-pixel extent as a special case later
//	P1 += (distanceSquared(P0, P1) < 0.0001f) ? 0.01f : 0.0f;
//	float2 delta = P1 - P0;
//
//	// Permute so that the primary iteration is in x to collapse
//	// all quadrant-specific DDA cases later
//	bool permute = false;
//	if (abs(delta.x) < abs(delta.y))
//	{
//		// This is a more-vertical line
//		permute = true;
//		delta = delta.yx;
//		P0 = P0.yx;
//		P1 = P1.yx;
//	}
//
//	float stepDir = sign(delta.x);
//	float invdx = stepDir / delta.x;
//
//	// Track the derivatives of Q and k
//	float3 dQ = (Q1 - Q0) * invdx;
//	float  dk = (k1 - k0) * invdx;
//
//	// Because we test 1/2 a texel forward along the ray, on the very last iteration
//	// the interpolation can go past the end of the ray. Use these bounds to clamp it.
//	float zMin = min(csEndPoint.z, csOrig.z);
//	float zMax = max(csEndPoint.z, csOrig.z);
//
//	float2 dP = float2(stepDir, delta.y * invdx);
//
//	// Scale derivatives by the desired pixel stride and then
//	// offset the starting values by the jitter fraction
//	dP *= stride;
//	dQ *= stride;
//	dk *= stride;
//	P0 += dP * jitter;
//	Q0 += dQ * jitter;
//	k0 += dk * jitter;
//
//	float4 PQk = float4(P0, Q0.z, k0);
//	float4 dPQk = float4(dP, dQ.z, dk);
//
//	// Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
//	float3 Q = Q0;
//
//	// Adjust end condition for iteration direction
//	float  end = P1.x * stepDir;
//
//	float k = k0;
//	float stepCount = 0.0f;
//	float prevZMaxEstimate = csOrig.z;
//	float rayZMin = prevZMaxEstimate;
//	float rayZMax = prevZMaxEstimate;
//	float sceneZMax = rayZMax + 100000;
//
//	//for (float2 P = P0;
//	//	((P.x * stepDir) <= end) && (stepCount < maxSteps) &&
//	//	((rayZMax < sceneZMax - zThickness) || (rayZMin > sceneZMax)) &&
//	//	(sceneZMax != 0);
//	//	P += dP, Q.z += dQ.z, k += dk, ++stepCount)
//	//{
//	//	rayZMin = prevZMaxEstimate;
//	//	rayZMax = (dQ.z * 0.5f + Q.z) / (dk * 0.5f + k);
//	//	prevZMaxEstimate = rayZMax;
//	//	if (rayZMin > rayZMax) {
//	//		float t = rayZMin;
//	//		rayZMin = rayZMax;
//	//		rayZMax = t;
//	//	}
//
//	//	hitPixel = permute ? P.yx : P;
//	//	// You may need hitPixel.y = csZBufferSize.y - hitPixel.y; here if your vertical axis
//	//	// is different than ours in screen space
//	//	sceneZMax = csZBuffer.Load(int3(hitPixel, 0));
//	//}
//
//
//	[loop]
//	for (; ((PQk.x * stepDir) <= end) &&
//		(stepCount < rayTraceMaxStep) &&
//		!IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax) &&
//		(sceneZMax != 0.0f);
//		PQk += dPQk, stepCount++)
//	{
//		if (any(hitPixel < 0.0) || any(hitPixel > 1.0))
//		{
//			return false;
//		}
//
//		rayZMin = prevZMaxEstimate;
//
//		// Compute the value at 1/2 step into the future
//		rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
//		rayZMax = clamp(rayZMax, zMin, zMax);
//
//		prevZMaxEstimate = rayZMax;
//
//		if (rayZMin > rayZMax)
//		{
//			float t = rayZMin;
//			rayZMin = rayZMax;
//			rayZMax = t;
//		}
//
//		hitPixel = permute ? PQk.yx : PQk.xy;
//		hitPixel *= 1 / csZBufferSize;
//
//		//sceneZMax = getLinearDepth(texture_depth.SampleLevel(sampler_point_clamp, hitPixel, 0).r);
//		sceneZMax = csZBuffer.SampleLevel(gNormalDepthSamplerState, hitPixel, 0).w;
//	}
//
//	// Advance Q based on the number of steps
//	Q.xy += dQ.xy * stepCount;
//	Q.z = PQk.z;
//	hitPoint = Q * (1.0f / PQk.w);
//
//	return IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
//}

//float4 main(VertexOut pin) : SV_Target
//{
//	// view space normal and depth (z-coord) of this pixel
//	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
//	float3 normal = normalize(NormalDepth.xyz);
//	float  depth  = NormalDepth.w;
//
//	float3 RayOrigin = GetFullViewPosition(pin.TexCoord, depth);
//	//float3 RayDir = normalize(reflect(normalize(RayOrigin), normal));
//	float3 RayDir = -2.0f * dot(normalize(RayOrigin), normal) * normal + normalize(RayOrigin);
//
//	float2 TexSize;
//	gNormalDepthMap.GetDimensions(TexSize.x, TexSize.y);
//
//	float2 HitPixel;
//	float3 HitPoint;
//
//	bool t = TraceScreenSpaceRay(
//		RayOrigin, // camera-space ray origin
//		RayDir, // camera-space ray direction
//		gProj, // projection matrix
//		gNormalDepthMap, // camera-space Z buffer
//		TexSize, // dimensions of Z buffer
//		0, // camera space thickness
//		gFrustumFarCorner[0].z, // near plane Z
//		1, // stride, step in horizontal or vertical pixels between samples
//		0, // jitter
//		10, // max steps
//		1000, // max camera-space distance
//		HitPixel, // hit pixel coordinates
//		HitPoint); // hit camera space location
//
//	float2 temp = saturate(HitPixel);
//
//	if (any(temp != HitPixel))
//	{
//		//t = 0;
//	}
//
//	//return t;
//	return float4(HitPixel, 0, t);
//}


// GPU Pro5: Advanced Rendering Techniques -> Hi-Z Screen-Space Cone-Traced Reflections (Yasin Uludag)
// http://bitsquid.blogspot.com/2017/08/notes-on-screen-space-hiz-tracing.html


// 4th ATTEMPT
// https://sakibsaikia.github.io/graphics/2016/12/26/Screen-Space-Reflection-in-Killing-Floor-2.html

float3 GetFullWorldPosition(float4 PositionNDC)
{
	float4 PositionW = mul(gViewProjInverse, PositionNDC);
	PositionW.xyz /= PositionW.w;
	PositionW.w = 1;
	return PositionW;
}

#define MAX_REFLECTION_RAY_MARCH_STEP 0.02f
#define NUM_RAY_MARCH_SAMPLES 32
#define NUM_BINARY_SEARCH_SAMPLES 6
#define RAY_MARH_BIAS 0.001f

float GetDitherOffset(float2 uv)
{
	uv = floor(frac(uv) * 4);

	float4x4 dither =
	{
		{  0,  8,  2, 10 },
		{ 12,  4, 14,  6 },
		{  3, 11,  1,  9 },
		{ 15,  7, 13,  5 },
	};

	return dither[uv.x][uv.y];
}

float GetReflection(
	float3 ScreenSpaceReflectionVec,
	float3 ScreenSpacePos,
	out float3 ReflectionColor)
{
	// Raymarch in the direction of the ScreenSpaceReflectionVec until you get an intersection with your z buffer
	for (int RayStepIdx = 1; RayStepIdx < NUM_RAY_MARCH_SAMPLES; RayStepIdx++)
	{
		float3 PrevRaySample = ((RayStepIdx - 1) * MAX_REFLECTION_RAY_MARCH_STEP) * ScreenSpaceReflectionVec + ScreenSpacePos;
		float3 RaySample = (RayStepIdx * MAX_REFLECTION_RAY_MARCH_STEP) * ScreenSpaceReflectionVec + ScreenSpacePos;

		// Dithered offset for raymarching to prevent banding artifacts
		float DitherTilingFactor = 1000;
		//float DitherOffset = DitherTexture.SampleLevel(InUV * DitherTilingFactor, 0).r * 0.01f + RAY_MARH_BIAS;
		float DitherOffset = GetDitherOffset(ScreenSpacePos.xy * DitherTilingFactor) * 0.01f + RAY_MARH_BIAS;
		RaySample += DitherOffset * ScreenSpaceReflectionVec;

		float ZBufferVal = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, RaySample.xy, 0).w;

		float2 UVSamplingAttenuation = smoothstep(0.05, 0.1, RaySample.xy) * (1 - smoothstep(0.95, 1, RaySample.xy));
		UVSamplingAttenuation.x *= UVSamplingAttenuation.y;

		if (UVSamplingAttenuation.x > 0)
		{
			// sample z-buffer and perform intersection check
		}
		else
		{
			return 0;
		}

		if (RaySample.z > ZBufferVal)
		{
			// binary search
			{
				float3 MinRaySample = PrevRaySample;
				float3 MaxRaySample = RaySample;
				float3 MidRaySample;

				for (int i = 0; i < NUM_BINARY_SEARCH_SAMPLES; i++)
				{
					MidRaySample = lerp(MinRaySample, MaxRaySample, 0.5);
					float ZBufferVal = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, MidRaySample.xy, 0).w;

					if (MidRaySample.z > ZBufferVal)
					{
						MaxRaySample = MidRaySample;
					}
					else
					{
						MinRaySample = MidRaySample;
					}
				}

				ReflectionColor = float3(MidRaySample.xy, 0);
			}

			return 1;
		}
	}

	return 0;
}

// Constant offset to make sure you cross to the next cell
#define CELL_STEP_OFFSET 0.05

void StepThroughCell(inout float3 RaySample, float3 RayDir, int MipLevel)
{
	// Size of current mip 
	int2 MipSize; // = int2(BufferSize) >> MipLevel;
	float NumDepthMips;
	gHierarchicalDepthBuffer.GetDimensions(MipLevel, MipSize.x, MipSize.y, NumDepthMips);

	// UV converted to index in the mip
	float2 MipCellIndex = RaySample.xy * float2(MipSize);

	//
	// Find the cell boundary UV based on the direction of the ray
	// Take floor() or ceil() depending on the sign of RayDir.xy
	//
	float2 BoundaryUV;
	BoundaryUV.x = (RayDir.x > 0 ? ceil(MipCellIndex.x) : floor(MipCellIndex.x)) / float(MipSize.x);
	BoundaryUV.y = (RayDir.y > 0 ? ceil(MipCellIndex.y) : floor(MipCellIndex.y)) / float(MipSize.y);

	//
	// We can now represent the cell boundary as being formed by the intersection of 
	// two lines which can be represented by 
	//
	// x = BoundaryUV.x
	// y = BoundaryUV.y
	//
	// Intersect the parametric equation of the Ray with each of these lines
	//
	float2 t;
	t.x = (BoundaryUV.x - RaySample.x) / RayDir.x;
	t.y = (BoundaryUV.y - RaySample.y) / RayDir.y;

	// Pick the cell intersection that is closer, and march to that cell
	if (abs(t.x) < abs(t.y))
	{
		RaySample += (t.x + CELL_STEP_OFFSET / MipSize.x) * RayDir;
	}
	else
	{
		RaySample += (t.y + CELL_STEP_OFFSET / MipSize.y) * RayDir;
	}
}

float GetReflectionHiZ(
	float3 ScreenSpaceReflectionVec,
	float3 ScreenSpacePos,
	out float3 ReflectionColor)
{
	float NumDepthMips;
	uint2 size;
	gHierarchicalDepthBuffer.GetDimensions(0, size.x, size.y, NumDepthMips);

	float3 RaySample = ScreenSpacePos;

	int LoopLimit = 1000;

	int MipLevel = 0;
	while (MipLevel > -1 && MipLevel < (NumDepthMips - 1) && LoopLimit > 0)
	{
		LoopLimit--;

		// Cross a single texel in the HZB for the current MipLevel
		StepThroughCell(RaySample, ScreenSpaceReflectionVec, MipLevel);

		// Constrain raymarch UV to (0-1) range with a falloff
		float2 UVSamplingAttenuation = smoothstep(0.05, 0.1, RaySample.xy) * (1 - smoothstep(0.95, 1, RaySample.xy));

		if (any(UVSamplingAttenuation))
		{
			float ZBufferValue = gHierarchicalDepthBuffer.SampleLevel(gHierarchicalDepthBufferSamplerState, RaySample.xy, MipLevel).r;

			if (RaySample.z < ZBufferValue)
			{
				// If we did not intersect, perform successive test on the next
				// higher mip level (and thus take larger steps)
				MipLevel++;
			}
			else
			{
				if (MipLevel == 0)
				{
					//// binary search
					//{
					//	float t = (RaySample.z - ZBufferValue) / ScreenSpaceReflectionVec.z;
					//	float3 MinRaySample = RaySample - ScreenSpaceReflectionVec * t;
					//	float3 MaxRaySample = RaySample;
					//	float3 MidRaySample;

					//	for (int i = 0; i < NUM_BINARY_SEARCH_SAMPLES; i++)
					//	{
					//		MidRaySample = lerp(MinRaySample, MaxRaySample, 0.5);
					//		float ZBufferVal = gHierarchicalDepthBuffer.SampleLevel(gHierarchicalDepthBufferSamplerState, MidRaySample.xy, MipLevel).r;

					//		if (MidRaySample.z > ZBufferVal)
					//		{
					//			MaxRaySample = MidRaySample;
					//		}
					//		else
					//		{
					//			MinRaySample = MidRaySample;
					//		}
					//	}

					//	ReflectionColor = float3(MidRaySample.xy, 0);
					//	return 1;
					//}

					ReflectionColor = float3(RaySample.xy, 0);
					return 1;
				}

				// If we intersected, pull back the ray to the point of intersection (for that miplevel)
				float t = (RaySample.z - ZBufferValue) / ScreenSpaceReflectionVec.z;
				RaySample -= ScreenSpaceReflectionVec * t;

				// And, then perform successive test on the next lower mip level.
				// Once we've got a valid intersection with mip 0, we've found our intersection point
				MipLevel--;
			}
		}
		else
		{
			//break;
			return 0;
		}
	}

	float ZBufferVal = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, RaySample.xy, 0).w;

	//if (RaySample.z > ZBufferVal)
	{
		// binary search
		{
			//float3 MinRaySample = PrevRaySample;
			//float3 MaxRaySample = RaySample;
			//float3 MidRaySample;

			//for (int i = 0; i < NUM_BINARY_SEARCH_SAMPLES; i++)
			//{
			//	MidRaySample = lerp(MinRaySample, MaxRaySample, 0.5);
			//	float ZBufferVal = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, MidRaySample.xy, 0).w;

			//	if (MidRaySample.z > ZBufferVal)
			//	{
			//		MaxRaySample = MidRaySample;
			//	}
			//	else
			//	{
			//		MinRaySample = MidRaySample;
			//	}
			//}

			//ReflectionColor = float3(MidRaySample.xy, 0);
			ReflectionColor = float3(RaySample.xy, 0);
		}

		//return 1;
	}

	return 0;
}

// http://bitsquid.blogspot.com/2017/08/notes-on-screen-space-hiz-tracing.html

#define HIZ_START_LEVEL 2
#define HIZ_STOP_LEVEL 8
#define HIZ_MAX_LEVEL 8
#define MAX_ITERATIONS 32

float2 cell(float2 ray, float2 cell_count)
{
	return floor(ray.xy * cell_count);
}

float2 cell_count(float level)
{
	float2 size;
	gHierarchicalDepthBuffer.GetDimensions(size.x, size.y);

	return size / (level == 0.0 ? 1.0 : exp2(level));
}

float3 intersect_cell_boundary(float3 pos, float3 dir, float2 cell_id, float2 cell_count, float2 cross_step, float2 cross_offset)
{
	float2 cell_size = 1.0 / cell_count;
	float2 planes = cell_id / cell_count + cell_size * cross_step;

	float2 solutions = (planes - pos) / dir.xy;
	float3 intersection_pos = pos + dir * min(solutions.x, solutions.y);

	intersection_pos.xy += (solutions.x < solutions.y) ? float2(cross_offset.x, 0.0) : float2(0.0, cross_offset.y);

	return intersection_pos;
}

bool crossed_cell_boundary(float2 cell_id_one, float2 cell_id_two)
{
	return (int)cell_id_one.x != (int)cell_id_two.x || (int)cell_id_one.y != (int)cell_id_two.y;
}

float minimum_depth_plane(float2 ray, float level, float2 cell_count)
{
	return gHierarchicalDepthBuffer.Load(int3(ray.xy * cell_count, level)).r;
}

float3 hi_z_trace(float3 p, float3 v, out uint iterations)
{
	float level = HIZ_START_LEVEL;
	float3 v_z = v / v.z;
	float2 hi_z_size = cell_count(level);
	float3 ray = p;

	float2 cross_step = float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
	float2 cross_offset = cross_step * 0.00001;
	cross_step = saturate(cross_step);

	float2 ray_cell = cell(ray.xy, hi_z_size.xy);
	ray = intersect_cell_boundary(ray, v, ray_cell, hi_z_size, cross_step, cross_offset);

	iterations = 0;
	while (level >= HIZ_STOP_LEVEL && iterations < MAX_ITERATIONS)
	{
		// get the cell number of the current ray
		float2 current_cell_count = cell_count(level);
		float2 old_cell_id = cell(ray.xy, current_cell_count);

		// get the minimum depth plane in which the current ray resides
		float min_z = minimum_depth_plane(ray.xy, level, current_cell_count);

		// intersect only if ray depth is below the minimum depth plane
		float3 tmp_ray = ray;
		if (v.z > 0)
		{
			float min_minus_ray = min_z - ray.z;
			tmp_ray = min_minus_ray > 0 ? ray + v_z * min_minus_ray : tmp_ray;
			float2 new_cell_id = cell(tmp_ray.xy, current_cell_count);

			if (crossed_cell_boundary(old_cell_id, new_cell_id))
			{
				tmp_ray = intersect_cell_boundary(ray, v, old_cell_id, current_cell_count, cross_step, cross_offset);
				level = min(HIZ_MAX_LEVEL, level + 2.0f);
			}
			else
			{
				if (level == 1 && abs(min_minus_ray) > 0.0001)
				{
					tmp_ray = intersect_cell_boundary(ray, v, old_cell_id, current_cell_count, cross_step, cross_offset);
					level = 2;
				}
			}
		}
		else if (ray.z < min_z)
		{
			tmp_ray = intersect_cell_boundary(ray, v, old_cell_id, current_cell_count, cross_step, cross_offset);
			level = min(HIZ_MAX_LEVEL, level + 2.0f);
		}

		ray.xyz = tmp_ray.xyz;
		--level;

		++iterations;
	}

	return ray;
}

float4 main(VertexOut pin) : sv_target
{
	float2 PixelUV = pin.TexCoord;
	float2 NDCPos = float2(+2.f, -2.f) * PixelUV + float2(-1.f, +1.f);

	// Prerequisites
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);

	float  DeviceZ = NormalDepth.w;
	float3 WorldPosition = GetFullWorldPosition(float4(NDCPos, DeviceZ, 1));
	float3 CameraVector = normalize(WorldPosition - gCameraPosition);
	float3 WorldNormal = mul(gViewInverse, float4(NormalDepth.xyz, 0)).xyz;

	// ScreenSpacePos --> (screencoord.xy, device_z)
	float4 ScreenSpacePos = float4(PixelUV, DeviceZ, 1.f);

	// Compute world space reflection vector
	float3 ReflectionVector = reflect(CameraVector, WorldNormal);

	float CameraFacingReflectionAttenuation = 1 - smoothstep(0.25, 0.5, dot(-CameraVector, ReflectionVector));

	// Reject if the reflection vector is pointing back at the viewer.
	if (CameraFacingReflectionAttenuation <= 0)
	{
		return float4(0, 0, 0, 0);
	}

	// Compute second sreen space point so that we can get the SS reflection vector
	float4 PointAlongReflectionVec = float4(10.f * ReflectionVector + WorldPosition, 1.f);
	float4 ScreenSpaceReflectionPoint = mul(gViewProj, PointAlongReflectionVec);
	ScreenSpaceReflectionPoint /= ScreenSpaceReflectionPoint.w;
	ScreenSpaceReflectionPoint.xy = ScreenSpaceReflectionPoint.xy * float2(+0.5, -0.5) + float2(0.5, 0.5);

	// Compute the sreen space reflection vector as the difference of the two screen space points
	float3 ScreenSpaceReflectionVec = normalize(ScreenSpaceReflectionPoint.xyz - ScreenSpacePos.xyz);

	float3 OutReflectionColor;
	//float a = GetReflection(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, OutReflectionColor);
	//float a = GetReflectionHiZ(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, OutReflectionColor);

	uint iterations = 0;
	float a = 1;
	OutReflectionColor = hi_z_trace(ScreenSpacePos.xyz, ScreenSpaceReflectionVec, iterations);

	float3 ReflectionNormal = WorldNormal;
	float DirectionBasedAttenuation = smoothstep(-1.7f, 0, dot(ReflectionNormal, -ReflectionVector));
	a *= DirectionBasedAttenuation;

	return float4(OutReflectionColor, a);
}

//// https://github.com/GPUOpen-Effects/FidelityFX-SSSR/blob/master/ffx-sssr/ffx_sssr.h
//
//#define FFX_SSSR_FLOAT_MAX 3.402823466e+38
//
//float3x3 CreateTBN(float3 N)
//{
//	float3 U;
//	if (abs(N.z) > 0.0)
//	{
//		float k = sqrt(N.y * N.y + N.z * N.z);
//		U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
//	}
//	else
//	{
//		float k = sqrt(N.x * N.x + N.y * N.y);
//		U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
//	}
//
//	float3x3 TBN;
//	TBN[0] = U;
//	TBN[1] = cross(N, U);
//	TBN[2] = N;
//	return transpose(TBN);
//}
//
//// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
//float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension)
//{
//	// Wrap arguments
//	pixel_i = pixel_i & 127u;
//	pixel_j = pixel_j & 127u;
//	sample_index = sample_index & 255u;
//	sample_dimension = sample_dimension & 255u;
//
//#ifndef SPP
//#define SPP 1
//#endif
//
//#if SPP == 1
//	const uint ranked_sample_index = sample_index ^ 0;
//#else
//	// xor index based on optimized ranking
//	const uint ranked_sample_index = sample_index ^ g_ranking_tile_buffer[sample_dimension + (pixel_i + pixel_j * 128u) * 8u];
//#endif
//
//	// Fetch value in sequence
//	uint value = g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];
//
//	// If the dimension is optimized, xor sequence value based on optimized scrambling
//	value = value ^ g_scrambling_tile_buffer[(sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u];
//
//	// Convert to float and return
//	return (value + 0.5f) / 256.0f;
//}
//
//float2 SampleRandomVector2D(uint2 pixel) {
//	float2 u = float2(
//		fmod(SampleRandomNumber(pixel.x, pixel.y, 0, 0u) + (g_frame_index & 0xFFu) * GOLDEN_RATIO, 1.0f),
//		fmod(SampleRandomNumber(pixel.x, pixel.y, 0, 1u) + (g_frame_index & 0xFFu) * GOLDEN_RATIO, 1.0f));
//	return u;
//}
//
//float3 SampleReflectionVector(float3 view_direction, float3 normal, float roughness, int2 dispatch_thread_id) {
//	float3x3 tbn_transform = CreateTBN(normal);
//	float3 view_direction_tbn = mul(-view_direction, tbn_transform);
//
//	float2 u = SampleRandomVector2D(dispatch_thread_id);
//
//	float3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
//#ifdef PERFECT_REFLECTIONS
//	sampled_normal_tbn = float3(0, 0, 1); // Overwrite normal sample to produce perfect reflection.
//#endif
//
//	float3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);
//
//	// Transform reflected_direction back to the initial space.
//	float3x3 inv_tbn_transform = transpose(tbn_transform);
//	return mul(reflected_direction_tbn, inv_tbn_transform);
//}
//
//// Mat must be able to transform origin from texture space to a linear space.
//float3 InvProjectPosition(float3 coord, float4x4 mat) {
//	coord.y = (1 - coord.y);
//	coord.xy = 2 * coord.xy - 1;
//	float4 projected = mul(float4(coord, 1), mat);
//	projected.xyz /= projected.w;
//	return projected.xyz;
//}
//
//// Origin and direction must be in the same space and mat must be able to transform from that space into clip space.
//float3 ProjectDirection(float3 origin, float3 direction, float3 screen_space_origin, float4x4 mat) {
//	float3 offsetted = ProjectPosition(origin + direction, mat);
//	return offsetted - screen_space_origin;
//}
//
//float3 ScreenSpaceToWorldSpace(float3 screen_space_position) {
//	return InvProjectPosition(screen_space_position, g_inv_view_proj);
//}
//
//float3 FFX_DNSR_Reflections_ScreenSpaceToViewSpace(float3 screen_uv_coord) {
//	return InvProjectPosition(screen_uv_coord, gProjInverse);
//}
//
//float2 FFX_SSSR_GetMipResolution(float2 screen_dimensions, int mip_level) {
//	return screen_dimensions * pow(0.5, mip_level);
//}
//
//// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
//float3 FFX_SSSR_HierarchicalRaymarch
//(
//	float3 origin,
//	float3 direction,
//	bool is_mirror,
//	float2 screen_size,
//	int most_detailed_mip,
//	uint min_traversal_occupancy,
//	uint max_traversal_intersections,
//	out bool valid_hit
//) 
//{
//	const float3 inv_direction = direction != 0 ? 1.0 / direction : FFX_SSSR_FLOAT_MAX;
//
//	// Start on mip with highest detail.
//	int current_mip = most_detailed_mip;
//
//	// Could recompute these every iteration, but it's faster to hoist them out and update them.
//	float2 current_mip_resolution = FFX_SSSR_GetMipResolution(screen_size, current_mip);
//	float2 current_mip_resolution_inv = rcp(current_mip_resolution);
//
//	// Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
//	// This means we ever so slightly over shoot into the next region. 
//	float2 uv_offset = 0.005 * exp2(most_detailed_mip) / screen_size;
//	uv_offset = direction.xy < 0 ? -uv_offset : uv_offset;
//
//	// Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
//	float2 floor_offset = direction.xy < 0 ? 0 : 1;
//
//	// Initially advance ray to avoid immediate self intersections.
//	float current_t;
//	float3 position;
//	FFX_SSSR_InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);
//
//	bool exit_due_to_low_occupancy = false;
//	int i = 0;
//	while (i < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy)
//	{
//		float2 current_mip_position = current_mip_resolution * position.xy;
//		float surface_z = FFX_SSSR_LoadDepth(current_mip_position, current_mip);
//		bool skipped_tile = FFX_SSSR_AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
//		current_mip += skipped_tile ? 1 : -1;
//		current_mip_resolution *= skipped_tile ? 0.5 : 2;
//		current_mip_resolution_inv *= skipped_tile ? 2 : 0.5;
//		++i;
//
//		exit_due_to_low_occupancy = !is_mirror && WaveActiveCountBits(true) <= min_traversal_occupancy;
//	}
//
//	valid_hit = (i <= max_traversal_intersections);
//
//	return position;
//}
//
//float FFX_SSSR_ValidateHit(float3 hit, float2 uv, float3 world_space_ray_direction, float2 screen_size, float depth_buffer_thickness)
//{
//	// Reject hits outside the view frustum
//	if (any(hit.xy < 0) || any(hit.xy > 1))
//	{
//		return 0;
//	}
//
//	// Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
//	float2 manhattan_dist = abs(hit.xy - uv);
//	if (all(manhattan_dist < (2 / screen_size)))
//	{
//		return 0;
//	}
//
//	// Don't lookup radiance from the background.
//	int2 texel_coords = int2(screen_size * hit.xy);
//	float surface_z = FFX_SSSR_LoadDepth(texel_coords / 2, 1);
//	if (surface_z == 1.0)
//	{
//		return 0;
//	}
//
//	// We check if we hit the surface from the back, these should be rejected.
//	float3 hit_normal = FFX_SSSR_LoadNormal(texel_coords);
//	if (dot(hit_normal, world_space_ray_direction) > 0)
//	{
//		return 0;
//	}
//
//	float3 view_space_surface = FFX_SSSR_ScreenSpaceToViewSpace(float3(hit.xy, surface_z));
//	float3 view_space_hit = FFX_SSSR_ScreenSpaceToViewSpace(hit);
//	float distance = length(view_space_surface - view_space_hit);
//
//	// Fade out hits near the screen borders
//	float2 fov = 0.05 * float2(screen_size.y / screen_size.x, 1);
//	float2 border = smoothstep(0, fov, hit.xy) * (1 - smoothstep(1 - fov, 1, hit.xy));
//	float vignette = border.x * border.y;
//
//	// We accept all hits that are within a reasonable minimum distance below the surface.
//	// Add constant in linear space to avoid growing of the reflections toward the reflected objects.
//	float confidence = 1 - smoothstep(0, depth_buffer_thickness, distance);
//	confidence *= confidence;
//
//	return vignette * confidence;
//}
//
//float4 main(VertexOut pin) : sv_target
//{
//	float2 PixelUV = pin.TexCoord;
//	float2 NDCPos = float2(+2.f, -2.f) * PixelUV + float2(-1.f, +1.f);
//
//	// Prerequisites
//	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
//
//	float  DeviceZ = NormalDepth.w;
//	float3 WorldPosition = GetFullWorldPosition(float4(NDCPos, DeviceZ, 1));
//	float3 CameraVector = normalize(WorldPosition - gCameraPosition);
//	float3 WorldNormal = mul(gViewInverse, float4(NormalDepth.xyz, 0)).xyz;
//
//	// SSSR
//	{
//		uint g_most_detailed_mip = 0;
//
//		int2 coords = pin.PositionH.xy;
//
//		uint2 screen_size;
//		gNormalDepthMap.GetDimensions(screen_size.x, screen_size.y);
//
//		float2 uv = (coords + 0.5) / screen_size;
//
//		float3 world_space_normal = WorldNormal;
//		float roughness = 0;
//		bool is_mirror = false;
//
//		int most_detailed_mip = is_mirror ? 0 : g_most_detailed_mip;
//		float2 mip_resolution = FFX_SSSR_GetMipResolution(screen_size, most_detailed_mip);
//		float z = gHierarchicalDepthBuffer.SampleLevel(gHierarchicalDepthBufferSamplerState, uv * mip_resolution, most_detailed_mip).r;
//
//		float3 screen_uv_space_ray_origin = float3(uv, z);
//		float3 view_space_ray = FFX_DNSR_Reflections_ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
//		float3 view_space_ray_direction = normalize(view_space_ray);
//
//		float3 view_space_surface_normal = mul(float4(normalize(world_space_normal), 0), g_view).xyz;
//		float3 view_space_reflected_direction = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, coords);
//		float3 screen_space_ray_direction = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, g_proj);
//
//		bool valid_hit = false;
//		float3 hit = FFX_SSSR_HierarchicalRaymarch(screen_uv_space_ray_origin, screen_space_ray_direction, is_mirror, screen_size, most_detailed_mip, g_min_traversal_occupancy, g_max_traversal_intersections, valid_hit);
//
//		float3 world_space_origin = ScreenSpaceToWorldSpace(screen_uv_space_ray_origin);
//		float3 world_space_hit = ScreenSpaceToWorldSpace(hit);
//		float3 world_space_ray = world_space_hit - world_space_origin.xyz;
//
//		float confidence = valid_hit ? FFX_SSSR_ValidateHit(hit, uv, world_space_ray, screen_size, g_depth_buffer_thickness) : 0;
//		float world_ray_length = length(world_space_ray);
//
//		float3 reflection_radiance = 0;
//		if (confidence > 0) {
//			// Found an intersection with the depth buffer -> We can lookup the color from lit scene.
//			reflection_radiance = g_lit_scene.Load(int3(screen_size * hit.xy, 0)).xyz;
//		}
//	}
//
//	// ScreenSpacePos --> (screencoord.xy, device_z)
//	float4 ScreenSpacePos = float4(PixelUV, DeviceZ, 1.f);
//
//	// Compute world space reflection vector
//	float3 ReflectionVector = reflect(CameraVector, WorldNormal);
//
//	float CameraFacingReflectionAttenuation = 1 - smoothstep(0.25, 0.5, dot(-CameraVector, ReflectionVector));
//
//	// Reject if the reflection vector is pointing back at the viewer.
//	if (CameraFacingReflectionAttenuation <= 0)
//	{
//		return float4(0, 0, 0, 0);
//	}
//
//	// Compute second sreen space point so that we can get the SS reflection vector
//	float4 PointAlongReflectionVec = float4(10.f * ReflectionVector + WorldPosition, 1.f);
//	float4 ScreenSpaceReflectionPoint = mul(gViewProj, PointAlongReflectionVec);
//	ScreenSpaceReflectionPoint /= ScreenSpaceReflectionPoint.w;
//	ScreenSpaceReflectionPoint.xy = ScreenSpaceReflectionPoint.xy * float2(+0.5, -0.5) + float2(0.5, 0.5);
//
//	// Compute the sreen space reflection vector as the difference of the two screen space points
//	float3 ScreenSpaceReflectionVec = normalize(ScreenSpaceReflectionPoint.xyz - ScreenSpacePos.xyz);
//
//	float3 OutReflectionColor;
//	//float a = GetReflection(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, OutReflectionColor);
//	//float a = GetReflectionHiZ(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, OutReflectionColor);
//
//	uint iterations = 0;
//	float a = 1;
//	OutReflectionColor = hi_z_trace(ScreenSpacePos.xyz, ScreenSpaceReflectionVec, iterations);
//
//	float3 ReflectionNormal = WorldNormal;
//	float DirectionBasedAttenuation = smoothstep(-1.7f, 0, dot(ReflectionNormal, -ReflectionVector));
//	a *= DirectionBasedAttenuation;
//
//	return float4(OutReflectionColor, a);
//}