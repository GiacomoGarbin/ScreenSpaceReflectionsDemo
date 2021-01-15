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

Texture2D gAlbedoTexture : register(t0);
Texture2D gNormalTexture : register(t1);
TextureCube gCubeMap : register(t2);
Texture2D gShadowTexture : register(t3);
Texture2D gAmbientTexture : register(t4);
#if ENABLE_SSR
Texture2D gReflectionsTexture : register(t5);
Texture2D gSceneAlbedoTexture : register(t6);
#endif // ENABLE_SSR

SamplerState gLinearSamplerState : register(s0);
SamplerComparisonState gShadowSamplerState : register(s1);

struct DomainOut
{
	float3 PositionW : POSITION;
	float4 PositionH : SV_POSITION;
	float3 NormalW   : NORMAL;
	float3 TangentW  : TANGENT;
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

void ComputeLightDirectional(
	Material material,
	LightDirectional light,
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = -light.direction; // the light vector aims opposite the light ray direction

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}
}

void ComputeLightPoint(
	Material material,
	LightPoint light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}

	float attenuation = 1 / dot(light.attenuation, float3(1, distance, distance * distance));

	diffuse *= attenuation;
	specular *= attenuation;
}

void ComputeLightSpot(
	Material material,
	LightSpot light,
	float3 P, // position
	float3 N, // normal
	float3 E, // eye vector
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0, 0, 0, 0);
	diffuse = float4(0, 0, 0, 0);
	specular = float4(0, 0, 0, 0);

	float3 L = light.position - P; // the light vector is oriented from the surface to the light

	float distance = length(L);

	if (distance > light.range) return;

	L /= distance; // normalize light vector

	ambient = material.ambient * light.ambient;

	float d = dot(L, N); // diffuse factor

	[flatten]
	if (d > 0)
	{
		diffuse = d * material.diffuse * light.diffuse;

		float3 R = reflect(-L, N);
		float s = pow(max(dot(R, E), 0), material.specular.w); // specular factor

		specular = s * material.specular * light.specular;
	}

	float cone = pow(max(dot(-L, light.direction), 0), light.cone);

	float attenuation = cone / dot(light.attenuation, float3(1, distance, distance * distance));

	ambient *= cone;
	diffuse *= attenuation;
	specular *= attenuation;
}

float3 NormalFromTangentToWorld(float3 NormalS, float3 NormalW, float3 TangentW)
{
	// from [0,1] to [-1,+1]
	float3 NormalT = 2 * NormalS - 1;

	float3 N = NormalW;
	float3 T = normalize(TangentW - dot(TangentW, N) * N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	return mul(NormalT, TBN);
}

float GetShadowFactor(float4 ShadowH)
{
	static const float ShadowMapSize  = 2048;
	static const float ShadowMapDelta = 1.0f / ShadowMapSize;

	// complete projection by doing division by w
	ShadowH.xyz /= ShadowH.w;

	// depth in NDC space
	float depth = ShadowH.z;

	// texel size
	const float dx = ShadowMapDelta;

	const float2 offsets[9] =
	{
		float2(-dx, -dx), float2(0, -dx), float2(dx, -dx),
		float2(-dx,   0), float2(0,   0), float2(dx,   0),
		float2(-dx, +dx), float2(0, +dx), float2(dx, +dx)
	};

	float light = 0;

	[unroll]
	for (int i = 0; i < 9; ++i)
	{
		light += gShadowTexture.SampleCmpLevelZero(gShadowSamplerState, ShadowH.xy + offsets[i], depth).r;
	}

	return light / 9.0f;
}

//float4 main(DomainOut pin) : SV_TARGET
float4 main(VertexOut pin) : SV_TARGET
{
	pin.NormalW = normalize(pin.NormalW); // interpolated normals can be unnormalized

#if ENABLE_SPHERE_TEXCOORD
	const float PI = 3.14159265359f;
	float3 D = -pin.NormalW;

	float u = 0.5f + atan2(D.x, D.z) / (2 * PI);
	float v = 0.5f - asin(D.y) / PI;
	pin.TexCoord = float2(u, v);

	// TODO : compute sphere tangent in the shader

#endif // ENABLE_SPHERE_TEXCOORD

	float3 E = gEyePositionW - pin.PositionW; // the eye vector is oriented from the surface to the eye position
	float DistToEye = length(E);
	E /= DistToEye;

	float4 TextureColor = float4(1, 1, 1, 1);

#if ENABLE_TEXTURE
	TextureColor = gAlbedoTexture.Sample(gLinearSamplerState, pin.TexCoord);
#if ENABLE_ALPHA_CLIPPING
	clip(TextureColor.a - 0.1f);
#endif // ENABLE_ALPHA_CLIPPING
#endif // ENABLE_TEXTURE

#if ENABLE_NORMAL_MAPPING
	float3 N = gNormalTexture.Sample(gLinearSamplerState, pin.TexCoord).rgb;
	pin.NormalW = NormalFromTangentToWorld(N, pin.NormalW, pin.TangentW);
#endif // ENABLE_NORMAL_MAPPING

	float4 color = TextureColor;

	// if light
	{
		float4 ambient = float4(0, 0, 0, 0);
		float4 diffuse = float4(0, 0, 0, 0);
		float4 specular = float4(0, 0, 0, 0);

		// only the first light casts a shadow
		float3 shadow = float3(1, 1, 1);
		shadow[0] = GetShadowFactor(pin.ShadowH);

		// finish texture projection and sample ambient map
		pin.AmbientH /= pin.AmbientH.w;
		float AmbientFactor = gAmbientTexture.SampleLevel(gLinearSamplerState, pin.AmbientH.xy, 0).r;

		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			float4 a, d, s;

			ComputeLightDirectional(gMaterial, gLights[i], pin.NormalW, E, a, d, s);
			ambient  += a * AmbientFactor;
			diffuse  += d * shadow[i];
			specular += s * shadow[i];
		}

		color = TextureColor * (ambient + diffuse) + specular;

#if ENABLE_REFLECTION
		float3 ReflectVec = reflect(-E, pin.NormalW);
		//float3 RefractVec = refract(-E, pin.NormalW, 1);
		float4 ReflectCol = gCubeMap.Sample(gLinearSamplerState, ReflectVec);
		color += gMaterial.reflect * ReflectCol;
#endif // ENABLE_REFLECTION

#if ENABLE_SSR
		{
			//float2 scale = float2(1 / gTexCoordTransform._11, 1 / gTexCoordTransform._22);
			//float2 TexCoord = scale * pin.TexCoord;


			float2 TexSize;
			gSceneAlbedoTexture.GetDimensions(TexSize.x, TexSize.y);
			float2 TexCoord = pin.PositionH.xy / TexSize;

			float4 RayPayload = gReflectionsTexture.Sample(gLinearSamplerState, TexCoord);
			//float4 ReflectCol = RayPayload.z ? gSceneAlbedoTexture.Sample(gLinearSamplerState, RayPayload.xy) : 0;
			float4 ReflectCol = gSceneAlbedoTexture.Sample(gLinearSamplerState, RayPayload.xy);
			//ReflectCol.a = 1;
			//color += gMaterial.reflect * ReflectCol;
			color += gMaterial.reflect * float4(lerp(float3(0,0,0), ReflectCol.rgb, RayPayload.w), 1);
			//color = ReflectCol;
		}
#endif // ENABLE_SSR
	}

#if ENABLE_FOG
	float t = saturate((DistToEye - gFogStart) / gFogRange);
	color = lerp(color, gFogColor, t);
#endif // ENABLE_FOG

	color.a = TextureColor.a * gMaterial.diffuse.a;

	return color;
}