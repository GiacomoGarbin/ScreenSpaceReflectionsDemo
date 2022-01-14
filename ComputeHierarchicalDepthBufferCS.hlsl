Texture2D<float> gDepthInput : register(t0);
RWTexture2D<float> gDepthOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
	uint3 size; // width, height, mips
	gDepthOutput.GetDimensions(size.x, size.y, size.z);

	// copia depth in mip 0






	uint2 thisLevelTexelCoord = DispatchThreadID.xy;
	uint2 previousLevelBaseTexelCoord = 2 * thisLevelTexelCoord;

	float4 depthTexelValues;
	depthTexelValues.x = gDepthInput[previousLevelBaseTexelCoord + uint2(0, 0)];
	depthTexelValues.y = gDepthInput[previousLevelBaseTexelCoord + uint2(1, 0)];
	depthTexelValues.z = gDepthInput[previousLevelBaseTexelCoord + uint2(1, 1)];
	depthTexelValues.w = gDepthInput[previousLevelBaseTexelCoord + uint2(0, 1)];

	float minDepth = min(min(depthTexelValues.x, depthTexelValues.y), min(depthTexelValues.z, depthTexelValues.w));

	bool shouldIncludeExtraColumnFromPreviousLevel = ((u_previousLevelDimensions.x & 1) != 0);
	bool shouldIncludeExtraRowFromPreviousLevel = ((u_previousLevelDimensions.y & 1) != 0);

	if (shouldIncludeExtraColumnFromPreviousLevel)
	{
		float2 extraColumnTexelValues;
		extraColumnTexelValues.x = gDepthInput[previousLevelBaseTexelCoord + uint2(2, 0)];
		extraColumnTexelValues.y = gDepthInput[previousLevelBaseTexelCoord + uint2(2, 1)];

		if (shouldIncludeExtraRowFromPreviousLevel)
		{
			float cornerTexelValue = gDepthInput[previousLevelBaseTexelCoord + uint2(2, 2)];
			minDepth = min(minDepth, cornerTexelValue);
		}

		minDepth = min(minDepth, min(extraColumnTexelValues.x, extraColumnTexelValues.y));
	}

	if (shouldIncludeExtraRowFromPreviousLevel)
	{
		float2 extraRowTexelValues;
		extraRowTexelValues.x = gDepthInput[previousLevelBaseTexelCoord + uint2(0, 2)];
		extraRowTexelValues.y = gDepthInput[previousLevelBaseTexelCoord + uint2(1, 2)];

		minDepth = min(minDepth, min(extraRowTexelValues.x, extraRowTexelValues.y));
	}

	gDepthOutput[DispatchThreadID.xy] = minDepth;
}