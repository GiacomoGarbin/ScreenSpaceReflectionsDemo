cbuffer mAmbientMapComputeCB : register(b0)
{
	float4x4 gProjTexture;
	float4   gSampleOffset[14];
	float4   gFrustumFarCorner[4];

	// coordinates given in view space
	float    gOcclusionRadius;
	float    gOcclusionFadeStart;
	float    gOcclusionFadeEnd;
	float    gSurfaceEpsilon;
};

struct VertexOut
{
	float4 PositionH  : SV_POSITION;
	float3 ToFarPlane : TEXCOORD0;
	float2 TexCoord   : TEXCOORD1;
};

Texture2D gNormalDepthMap : register(t0);
Texture2D gRandomVectorMap : register(t1);

SamplerState gNormalDepthSamplerState : register(s2);
SamplerState gRandomVectorSamplerState : register(s3);

// determines how much the sample point q occludes the point p as a function of dz
float OcclusionFunction(float dz)
{
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.

	float occlusion = 0.0f;
	if (dz > gSurfaceEpsilon)
	{
		float OcclusionFadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;

		// linearly decrease occlusion from 1 to 0 as dz goes from gOcclusionFadeStart to gOcclusionFadeEnd
		occlusion = saturate((gOcclusionFadeEnd - dz) / OcclusionFadeLength);
	}

	return occlusion;
}

float4 main(VertexOut pin) : SV_Target
{
	// p -> the point we are computing the ambient occlusion for
	// n -> normal vector at p
	// q -> a random offset from p
	// r -> a potential occluder that might occlude p

	// view space normal and depth (z-coord) of this pixel
	float4 NormalDepth = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, pin.TexCoord, 0);

	float3 n = NormalDepth.xyz;
	float pz = NormalDepth.w;

	// reconstruct full view space position (x,y,z)
	// find t such that p = t*pin.ToFarPlane
	// p.z = t*pin.ToFarPlane.z ==> t = p.z / pin.ToFarPlane.z
	float3 p = (pz / pin.ToFarPlane.z) * pin.ToFarPlane;

	// extract random vector and map from [0,1] to [-1, +1]
	float3 RandomVector = 2.0f * gRandomVectorMap.SampleLevel(gRandomVectorSamplerState, 4.0f * pin.TexCoord, 0).rgb - 1.0f;

	float occlusion = 0;

	// sample neighboring points about p in the hemisphere oriented by n
	[unroll]
	for (int i = 0; i < 14; ++i)
	{
		// the offset vectors are fixed and uniformly distributed
		// (so that our offset vectors do not clump in the same direction)
		// if we reflect them about a random vector then we get a random uniform distribution of offset vectors
		float3 offset = reflect(gSampleOffset[i].xyz, RandomVector);

		// flip offset vector if it is behind the plane defined by (p, n)
		float flip = sign(dot(offset, n));

		// sample a point near p within the occlusion radius
		float3 q = p + flip * gOcclusionRadius * offset;

		// project q and generate projective tex-coords
		float4 Q = mul(gProjTexture, float4(q, 1.0f));
		Q /= Q.w;

		// Find the nearest depth value along the ray from the eye to q
		// (this is not the depth of q, as q is just an arbitrary point near p and might occupy empty space)
		// to find the nearest depth we look it up in the depthmap
		float rz = gNormalDepthMap.SampleLevel(gNormalDepthSamplerState, Q.xy, 0).a;

		// reconstruct full view space position r = (rx,ry,rz)
		// we know r lies on the ray of q, so there exists a t such that r = t*q
		// r.z = t*q.z ==> t = r.z / q.z
		float3 r = (rz / q.z) * q;

		// Test whether r occludes p.
		//  * The product dot(n, normalize(r - p)) measures how much in front
		//    of the plane(p,n) the occluder point r is.  The more in front it is, the
		//    more occlusion weight we give it.  This also prevents self shadowing where 
		//    a point r on an angled plane (p,n) could give a false occlusion since they
		//    have different depth values with respect to the eye.
		//  * The weight of the occlusion is scaled based on how far the occluder is from
		//    the point we are computing the occlusion of.  If the occluder r is far away
		//    from p, then it does not occlude it.

		float dz = p.z - r.z;
		float dp = max(dot(n, normalize(r - p)), 0.0f);

		occlusion += dp * OcclusionFunction(dz);
	}

	occlusion /= 14;

	// sharpen the contrast of the SSAO map
	return saturate(pow(1.0f - occlusion, 4.0f));
}