struct VertexOut
{
    float4 PositionH : SV_POSITION;
    float2 TexCoord  : TEXCOORD;
};

Texture2D<float> gDepthInput : register(t0);
//SamplerState gSamplerState : register(s0);

float main(VertexOut pin) : SV_Target
{
    //float r = gTexture.SampleLevel(gSamplerState, pin.TexCoord, 0).r;
    //return float4(r, 0, 0, 1);

	uint2 u_previousLevelDimensions;
	gDepthInput.GetDimensions(u_previousLevelDimensions.x, u_previousLevelDimensions.y);

	uint2 thisLevelTexelCoord = pin.PositionH.xy;
	uint2 previousLevelBaseTexelCoord = 2 * thisLevelTexelCoord;

	float4 depthTexelValues;
	depthTexelValues.x = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(0, 0), 0));
	depthTexelValues.y = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(1, 0), 0));
	depthTexelValues.z = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(1, 1), 0));
	depthTexelValues.w = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(0, 1), 0));

	float minDepth = min(min(depthTexelValues.x, depthTexelValues.y), min(depthTexelValues.z, depthTexelValues.w));

	bool shouldIncludeExtraColumnFromPreviousLevel = ((u_previousLevelDimensions.x & 1) != 0);
	bool shouldIncludeExtraRowFromPreviousLevel = ((u_previousLevelDimensions.y & 1) != 0);

	if (shouldIncludeExtraColumnFromPreviousLevel)
	{
		float2 extraColumnTexelValues;
		extraColumnTexelValues.x = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(2, 0), 0));
		extraColumnTexelValues.y = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(2, 1), 0));

		if (shouldIncludeExtraRowFromPreviousLevel)
		{
			float cornerTexelValue = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(2, 2), 0));
			minDepth = min(minDepth, cornerTexelValue);
		}

		minDepth = min(minDepth, min(extraColumnTexelValues.x, extraColumnTexelValues.y));
	}

	if (shouldIncludeExtraRowFromPreviousLevel)
	{
		float2 extraRowTexelValues;
		extraRowTexelValues.x = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(0, 2), 0));
		extraRowTexelValues.y = gDepthInput.Load(uint3(previousLevelBaseTexelCoord + uint2(1, 2), 0));

		minDepth = min(minDepth, min(extraRowTexelValues.x, extraRowTexelValues.y));
	}

	return minDepth;
}
