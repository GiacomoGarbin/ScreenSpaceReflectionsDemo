cbuffer ConstantBuffer : register(b0)
{
	float4   gFrustumFarCorner[4]; // just need 3 floats : half width, half height and far z (maybe near z too)
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

float distanceSquared(float2 a, float2 b) { a -= b; return dot(a, a); }

static const float rayTraceMaxStep = 512.0f; // Maximum number of iterations. Higher gives better images but may be slow.
static const float rayTraceThicknessOffset = 0.01f; // Increse or decrease thickness for each pixels in the depth buffer. [- / +]
static const float rayTraceThicknessBias = 0.1f; // Bias to control the growth of the thickness.
static const bool raytraceThicknessInfinite = false; // Use infinite thickness for maximum performance, but may not be suitable for most scenes.
static const float rayTraceStrideCutoff = 100.0f; // More distant pixels are smaller in screen space. This value tells at what point to

bool IntersectsDepthBuffer(float sceneZMax, float rayZMin, float rayZMax)
{
	// Increase thickness along distance. 
	float thickness = max(sceneZMax * rayTraceThicknessBias + rayTraceThicknessOffset, 1.0);

	// Effectively remove line/tiny artifacts, mostly caused by Zbuffers precision.
	float depthScale = min(1.0f, sceneZMax / rayTraceStrideCutoff);
	sceneZMax += lerp(0.05f, 0.0f, depthScale);

	if (raytraceThicknessInfinite)
		return (rayZMin >= sceneZMax);
	else
		return (rayZMin >= sceneZMax) && (rayZMax - thickness <= sceneZMax);
}

// Returns true if the ray hit something
bool TraceScreenSpaceRay(
	// Camera-space ray origin, which must be within the view volume
	float3 csOrig,

	// Unit length camera-space ray direction
	float3 csDir,

	// A projection matrix that maps to pixel coordinates (not [-1, +1]
	// normalized device coordinates)
	float4x4 proj,

	// The camera-space Z buffer (all negative values)
	Texture2D csZBuffer,

	// Dimensions of csZBuffer
	float2 csZBufferSize,

	// Camera space thickness to ascribe to each pixel in the depth buffer
	float zThickness,

	// (Negative number)
	float nearPlaneZ,

	// Step in horizontal or vertical pixels between samples. This is a float
	// because integer math is slow on GPUs, but should be set to an integer >= 1
	float stride,

	// Number between 0 and 1 for how far to bump the ray in stride units
	// to conceal banding artifacts
	float jitter,

	// Maximum number of iterations. Higher gives better images but may be slow
	const float maxSteps,

	// Maximum camera-space distance to trace before returning a miss
	float maxDistance,

	// Pixel coordinates of the first intersection with the scene
	out float2 hitPixel,

	// Camera space location of the ray hit
	out float3 hitPoint)
{
	hitPixel = 0;
	hitPoint = 0;

	// Clip to the near plane    
	float rayLength = ((csOrig.z + csDir.z * maxDistance) < nearPlaneZ) ? (nearPlaneZ - csOrig.z) / csDir.z : maxDistance;
	float3 csEndPoint = csOrig + csDir * rayLength;

	// Project into homogeneous clip space
	float4 H0 = mul(proj, float4(csOrig, 1.0f));
	float4 H1 = mul(proj, float4(csEndPoint, 1.0f));

	float k0 = 1.0f / H0.w;
	float k1 = 1.0f / H1.w;

	// The interpolated homogeneous version of the camera-space points  
	float3 Q0 = csOrig * k0;
	float3 Q1 = csEndPoint * k1;

	// Screen-space endpoints
	float2 P0 = H0.xy * k0;
	float2 P1 = H1.xy * k1;

	// Project to pixel
	P0 = P0 * float2(+0.5f, -0.5f) + float2(0.5f, 0.5f);
	P1 = P1 * float2(+0.5f, -0.5f) + float2(0.5f, 0.5f);

	//return float4(P0, 0, 1);

	P0.xy *= csZBufferSize.xy;
	P1.xy *= csZBufferSize.xy;

	// If the line is degenerate, make it cover at least one pixel
	// to avoid handling zero-pixel extent as a special case later
	P1 += (distanceSquared(P0, P1) < 0.0001f) ? 0.01f : 0.0f;
	float2 delta = P1 - P0;

	// Permute so that the primary iteration is in x to collapse
	// all quadrant-specific DDA cases later
	bool permute = false;
	if (abs(delta.x) < abs(delta.y))
	{
		// This is a more-vertical line
		permute = true;
		delta = delta.yx;
		P0 = P0.yx;
		P1 = P1.yx;
	}

	float stepDir = sign(delta.x);
	float invdx = stepDir / delta.x;

	// Track the derivatives of Q and k
	float3 dQ = (Q1 - Q0) * invdx;
	float  dk = (k1 - k0) * invdx;

	// Because we test 1/2 a texel forward along the ray, on the very last iteration
	// the interpolation can go past the end of the ray. Use these bounds to clamp it.
	float zMin = min(csEndPoint.z, csOrig.z);
	float zMax = max(csEndPoint.z, csOrig.z);

	float2 dP = float2(stepDir, delta.y * invdx);

	// Scale derivatives by the desired pixel stride and then
	// offset the starting values by the jitter fraction
	dP *= stride;
	dQ *= stride;
	dk *= stride;
	P0 += dP * jitter;
	Q0 += dQ * jitter;
	k0 += dk * jitter;

	float4 PQk = float4(P0, Q0.z, k0);
	float4 dPQk = float4(dP, dQ.z, dk);

	// Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
	float3 Q = Q0;

	// Adjust end condition for iteration direction
	float  end = P1.x * stepDir;

	float k = k0;
	float stepCount = 0.0f;
	float prevZMaxEstimate = csOrig.z;
	float rayZMin = prevZMaxEstimate;
	float rayZMax = prevZMaxEstimate;
	float sceneZMax = rayZMax + 100000;

	//for (float2 P = P0;
	//	((P.x * stepDir) <= end) && (stepCount < maxSteps) &&
	//	((rayZMax < sceneZMax - zThickness) || (rayZMin > sceneZMax)) &&
	//	(sceneZMax != 0);
	//	P += dP, Q.z += dQ.z, k += dk, ++stepCount)
	//{
	//	rayZMin = prevZMaxEstimate;
	//	rayZMax = (dQ.z * 0.5f + Q.z) / (dk * 0.5f + k);
	//	prevZMaxEstimate = rayZMax;
	//	if (rayZMin > rayZMax) {
	//		float t = rayZMin;
	//		rayZMin = rayZMax;
	//		rayZMax = t;
	//	}

	//	hitPixel = permute ? P.yx : P;
	//	// You may need hitPixel.y = csZBufferSize.y - hitPixel.y; here if your vertical axis
	//	// is different than ours in screen space
	//	sceneZMax = csZBuffer.Load(int3(hitPixel, 0));
	//}


	[loop]
	for (; ((PQk.x * stepDir) <= end) &&
		(stepCount < rayTraceMaxStep) &&
		!IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax) &&
		(sceneZMax != 0.0f);
		PQk += dPQk, stepCount++)
	{
		if (any(hitPixel < 0.0) || any(hitPixel > 1.0))
		{
			return false;
		}

		rayZMin = prevZMaxEstimate;

		// Compute the value at 1/2 step into the future
		rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
		rayZMax = clamp(rayZMax, zMin, zMax);

		prevZMaxEstimate = rayZMax;

		if (rayZMin > rayZMax)
		{
			float t = rayZMin;
			rayZMin = rayZMax;
			rayZMax = t;
		}

		hitPixel = permute ? PQk.yx : PQk.xy;
		hitPixel *= 1 / csZBufferSize;

		//sceneZMax = getLinearDepth(texture_depth.SampleLevel(sampler_point_clamp, hitPixel, 0).r);
		sceneZMax = csZBuffer.SampleLevel(gNormalDepthSamplerState, hitPixel, 0).w;
	}

	// Advance Q based on the number of steps
	Q.xy += dQ.xy * stepCount;
	Q.z = PQk.z;
	hitPoint = Q * (1.0f / PQk.w);

	return IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

float4 main(VertexOut pin) : SV_Target
{
	// view space normal and depth (z-coord) of this pixel
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);
	float3 normal = normalize(NormalDepth.xyz);
	float  depth  = NormalDepth.w;

	float3 RayOrigin = GetFullViewPosition(pin.TexCoord, depth);
	//float3 RayDir = normalize(reflect(normalize(RayOrigin), normal));
	float3 RayDir = -2.0f * dot(normalize(RayOrigin), normal) * normal + normalize(RayOrigin);

	float2 TexSize;
	gNormalDepthMap.GetDimensions(TexSize.x, TexSize.y);

	float2 HitPixel;
	float3 HitPoint;

	bool t = TraceScreenSpaceRay(
		RayOrigin, // camera-space ray origin
		RayDir, // camera-space ray direction
		gProj, // projection matrix
		gNormalDepthMap, // camera-space Z buffer
		TexSize, // dimensions of Z buffer
		0, // camera space thickness
		gFrustumFarCorner[0].z, // near plane Z
		1, // stride, step in horizontal or vertical pixels between samples
		0, // jitter
		10, // max steps
		1000, // max camera-space distance
		HitPixel, // hit pixel coordinates
		HitPoint); // hit camera space location

	float2 temp = saturate(HitPixel);

	if (any(temp != HitPixel))
	{
		//t = 0;
	}

	//return t;
	return float4(HitPixel, 0, t);
}



// 4th ATTEMPT

static float gThickness = 1;
static float gStride = 1;
static float gJitter = 0;
static float gMaxDistance = 1000;
static float gMaxSteps = 100;

bool RayTrace(
	float3 RayOrigin,
	float3 RayDir,
	out float2 HitTexCoord
)
{
	return false;
}