struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord  : TEXCOORD;
};

cbuffer BlurCB : register(b0)
{
	float gTexelWidth;
	float gTexelHeight;
	float2 padding;
};

Texture2D gNormalDepthMap : register(t0);
Texture2D gAmbientMap : register(t1);

SamplerState gBlurSamplerState : register(s4);

// global variables that are not marked static or extern are not compiled into the shader
static float gBlurWeights[11] = { 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f };
static float gBlurRadius = 5;

float4 main(VertexOut pin) : SV_Target
{
	float2 offset;
#if HORIZONTAL_BLUR
	offset = float2(gTexelWidth, 0.0f);
#else
	offset = float2(0.0f, gTexelHeight);
#endif

	// the center value always contributes to the sum
	float4 color = gBlurWeights[5] * gAmbientMap.SampleLevel(gBlurSamplerState, pin.TexCoord, 0);
	float total = gBlurWeights[5];

	float4 CenterNormalDepth = gNormalDepthMap.SampleLevel(gBlurSamplerState, pin.TexCoord, 0);
	
	for (float i = -gBlurRadius; i <= +gBlurRadius; ++i)
	{
		// we already added in the center weight
		if (i == 0) continue;

		float2 TexCoord = pin.TexCoord + i * offset;
		float4 NeighborNormalDepth = gNormalDepthMap.SampleLevel(gBlurSamplerState, TexCoord, 0);

		// if the center value and neighbor values differ too much (either in normal or depth),
		// then we assume we are sampling across a discontinuity, we discard such samples from the blur
		if (dot(NeighborNormalDepth.xyz, CenterNormalDepth.xyz) >= 0.8f && abs(NeighborNormalDepth.a - CenterNormalDepth.a) <= 0.2f)
		{
			float weight = gBlurWeights[i + gBlurRadius];
			// add neighbor pixel to the blur
			color += weight * gAmbientMap.SampleLevel(gBlurSamplerState, TexCoord, 0);
			total += weight;
		}
	}

	return color / total;
}